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
import type { VictoryLevel } from '../../core/ticket';
import type { BattleSim, Command, FieldEvent, FieldResult, Side, Zone } from '../core/types';
import { attackKills, CONVOY, gunMmOf } from './numbers';
import type {
  BoatDoing, ConvoyClass, ConvoyCommand, ConvoyCounts, ConvoyFleet, ConvoyKit, ConvoyOverExtra, ConvoySpot, ConvoyTally, ConvoyTicket, ConvoyUnit, ConvoyView, Dive, Echo, EscortDoing,
} from './types';

const B = CONVOY.boat, E = CONVOY.escort, D = CONVOY.detect, T = CONVOY.torpedo, H = CONVOY.hunt, S = CONVOY.surface;
/** A number from 0 to under 1 fixed by a whole number: looks only, never the battle's dice (as the fleet action's mix). */
export const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const chanceIn = (mean: number, dt: number): number => 1 - Math.exp(-dt / Math.max(1e-6, mean));
/** Battle seconds within which an escort finding again the boat it lost holds the same contact (no new report, no new line). */
const AGAIN = 120;
/** Battle seconds a side keeps a piece it has lost touch with at her last known spot (the faded mark; a hunt may still be sent there). */
const LAST_SEEN = 180;
/** The page's own notes, never the scene's: an order answered, a boat of the pack found, the first salvo, a contact sure lost. Left out of the whole picture. */
const PAGE_ONLY = new Set(['sea:order', 'sea:found', 'sea:attack', 'sea:lost']);
/**
 * A boat her commander ordered to evade goes back in once she has been unheard PRESS: under water where she is within
 * PRESS_AT of her firing station and can get within firing reach of a ship dived within DIVED_IN (acrossTrack). A boat
 * ordered to attack submerged comes up to run on the surface once she is farther than SURFACE_RUN from her station, or
 * once she cannot get in dived within DIVED_IN and no escort is near, or she has been down that long. (M.) Her captain's
 * own dives are the rules owner's (round 3, the radar nights' dive loop): found while no escort runs in on her, she
 * dives only to periscope depth, keeps closing and comes up again once free (boatHelm); hunted, she goes deep and waits
 * out the long quiet (E.lostFor).
 */
const PRESS = 600, PRESS_AT = 4750, SURFACE_RUN = 7000;
/** Metres a second a boat's keel rises (boatHelm), so the seconds a boat forced up takes to reach the surface. */
const RISE = 2;
/** Metres ahead of the ship she makes for that a boat ordered to attack submerged takes her place on the surface, to dive there (M). */
const UNDER_AHEAD = 1500;
/** Battle seconds a dived boat may take to get within firing reach of a ship, at her best heading, before she runs round on the surface instead (M). */
const DIVED_IN = 3600;
/** A battle second as the night's clock reads it, from dusk: '0:15', '2:56'. */
const clockOf = (sec: number): string => `${Math.floor(sec / 3600)}:${String(Math.floor((sec % 3600) / 60)).padStart(2, '0')}`;
/** A battle second as words from dusk, the way the after card says it: 'dusk', '3 minutes after dusk', '2:56 after dusk'. */
export function afterDusk(sec: number): string {
  const m = Math.floor(sec / 60);
  return m < 1 ? 'dusk' : m < 60 ? `${m} ${m === 1 ? 'minute' : 'minutes'} after dusk` : `${clockOf(sec)} after dusk`;
}
/** Metres as the screens write them (3,200 m). */
const metres = (m: number): string => `${String(Math.round(m)).replace(/\B(?=(\d{3})+(?!\d))/g, ',')} m`;
/** One piece's name, or several of one kind in one phrase: 'Escort 5', 'Escorts 2 and 5', 'Boats 1, 3 and 4'. */
export function namesOf(names: string[]): string {
  if (names.length <= 1) return names[0] ?? '';
  const head = names[0].replace(/ \d+$/, '');
  const same = names.every((n) => n.replace(/ \d+$/, '') === head && n !== head);
  const parts = same ? names.map((n) => n.slice(head.length + 1)) : names;
  return `${same ? `${head}s ` : ''}${parts.slice(0, -1).join(', ')} and ${parts[parts.length - 1]}`;
}

/**
 * How the night's boats lost went, and those forced up, in plain words (the after card's fate lines): 'None of the 6
 * boats was lost.', '3 of the 15 boats were lost: 2 sunk by depth charges and 1 after she was forced to the surface.',
 * '1 of the 6 boats was lost, sunk by depth charges.' A boat forced up is told once, on the line of those forced up,
 * with what became of her: '1 boat was forced to the surface and scuttled by her crew.' Every way but depth charges
 * below is a boat's forced up first (rammed, shelled, scuttled, taken): the rest of those forced up dived again and got
 * away, or were still up at dawn. One already on the surface when gunfire left her unable to dive was kept there, not
 * forced up ('kept'), and is said so.
 */
export function fateWords(tally: ConvoyTally, lost: number, of: number): string[] {
  const n = (k: number, one: string, many: string): string => `${k} ${k === 1 ? one : many}`;
  const and = (xs: string[]): string => xs.length > 1 ? `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}` : xs[0] ?? '';
  const surfaced = tally.rammed + tally.shelled + tally.scuttled + tally.captured;
  const below = Math.max(0, lost - surfaced), after = Math.min(lost, surfaced);
  // (one way only: said without its count, 'was lost, sunk by depth charges')
  const how = below && after ? `: ${below} sunk by depth charges and ${after} after ${after === 1 ? 'she was' : 'they were'} forced to the surface`
    : below ? `, ${below === 1 ? '' : lost === of ? 'each ' : 'all '}sunk by depth charges` : '';
  const lines = [of === 1 ? (lost ? `The boat was lost${how}.` : 'The boat was not lost.')
    : lost === 0 ? `None of the ${of} boats was lost.` : `${lost === of ? `All ${of} boats were` : `${lost} of the ${of} boats ${lost === 1 ? 'was' : 'were'}`} lost${how}.`];
  const up = Math.max(tally.forcedUp, surfaced + tally.escaped);
  if (!up) return lines;
  // What became of those forced up, each way said after 'forced to the surface and' where all went one way, else counted.
  const was = (k: number): string => k === 1 ? 'was' : 'were';
  const fates: [number, (k: number) => string, (k: number) => string][] = [
    [tally.scuttled, (k) => `scuttled by ${k === 1 ? 'her crew' : 'their crews'}`, (k) => `${was(k)} scuttled by ${k === 1 ? 'her crew' : 'their crews'}`],
    [tally.captured, (k) => `taken by ${k === 1 ? 'an escort' : 'escorts'}`, (k) => `${was(k)} taken by ${k === 1 ? 'an escort' : 'escorts'}`],
    [tally.rammed, () => 'rammed', (k) => `${was(k)} rammed`],
    [tally.shelled, () => 'sunk by gunfire', (k) => `${was(k)} sunk by gunfire`],
    [tally.escaped, () => 'dived again to get away', () => 'dived again and got away'],
    [up - surfaced - tally.escaped, (k) => `${was(k)} still there at dawn`, (k) => `${was(k)} still on the surface at dawn`],
  ];
  const had = fates.filter(([k]) => k > 0);
  const kept = Math.min(up, tally.kept);
  const head = `${n(up, 'boat was', 'boats were')} ${kept === 0 ? 'forced to the surface' : kept === up ? 'kept on the surface by gunfire' : 'forced to the surface or kept there by gunfire'}`;
  lines.push(had.length === 1 ? `${head} and ${had[0][1](up)}.` : `${head}: ${and(had.map(([k, , list]) => `${k} ${list(k)}`))}.`);
  return lines;
}

/**
 * How far the night went the winner's way, as the pack reads it (words only, as the winner is: the campaign books the
 * night by its ranks). The pack's score is the ships it sank, the escorts' three for each boat sunk and two for each
 * escort; the pack wins where its score is the higher and it sank more than a twentieth of the convoy (the rules owner,
 * round 3: a night that sinks fewer is the convoy getting through, the escorts' night). Its win is overwhelming where
 * it sank three tenths of the convoy and three times the escorts' score, clear at three twentieths and twice, costly
 * where it lost half its boats or more, else close. The escorts' win is overwhelming where the convoy came through
 * untouched, or nearly (a twentieth), and two boats or more were sunk; clear where the convoy came through untouched,
 * or the escorts' score is twice the pack's; else close.
 */
export function levelOfNight(sunk: number, ships: number, lostB: number, pack: number, lostE: number): VictoryLevel {
  const p = sunk, q = 3 * lostB + 2 * lostE;
  if (p > q && sunk > 0.05 * ships) return sunk >= 0.3 * ships && p >= 3 * q ? 'Heroic victory' : sunk >= 0.15 * ships && p >= 2 * q ? 'Decisive victory' : lostB * 2 >= pack ? 'Pyrrhic victory' : 'Close victory';
  if (sunk <= 0.05 * ships && lostB >= 2) return 'Crushing defeat';
  return sunk === 0 || q >= 2 * p ? 'Decisive defeat' : 'Close defeat';
}

interface Boat {
  u: ConvoyUnit; fleet: number; nat: string;
  doing: BoatDoing; arrive: number; from: number; homed: boolean;
  dive: Dive; keel: number; keelTo: number; under: number;
  tubes: number; load: number; reloadAt: number;
  harm: number; doomed: boolean; repairAt: number; scuttleAt: number; struck: number; nextGun: number; gunOn: number;
  side: number; ahead: number; until: number; lastHeard: number;
  sees: Set<number>; order: 'attack' | 'under' | 'dive' | 'deep' | 'surface' | 'evade' | 'gun' | 'leave' | null; target: number;
  /** The depth her commander last chose to attack at (Attack, Attack submerged, Dive, Surface), or none yet. */
  depth: 'surface' | 'under' | null;
  /** Her crew have left her (abandoned or given up): she fights no more and is no gun's target. */
  abandoned: boolean;
  /** Her change of depth still to be said: a boat coming up is said to be there only once her keel is (setDive). */
  saying: { dive: Dive; why: string; reason?: string; override: boolean; rising: boolean; quiet: boolean } | null;
  /** Whether she is down on a duck (her captain's own, to periscope depth, for an escort that found her or is closing), and whether its line went unsaid. */
  ducking: boolean; duckQuiet: boolean;
  /** The battle second she last came back up to the surface from a duck. */
  duckUp: number;
  /** Kept down by an escort in her sight where she cannot get in dived: drawing off out of its sight (boatHelm). */
  off: boolean;
}
interface Escort {
  u: ConvoyUnit; fleet: number; nat: string; kit: ConvoyKit;
  doing: EscortDoing; sx: number; sy: number;
  holds: number; echo: Echo | null; seenAt: number; huntFrom: number; dx: number; dy: number;
  openTo: number; nextGun: number; nextShell: number; harm: number;
  order: 'hunt' | 'screen' | 'rescue' | 'move' | null; target: number; tx: number; ty: number; until: number;
  attacks: number;
  /** The boat she was sent after and does not hold herself (-1 none): she makes for where her side knows her. */
  chase: number;
  /** The boat her hunt is after (-1 none): the one she was sent after or began on, kept while she searches for her after losing her. */
  prey: number;
  /** When she last lost each boat (found again within AGAIN, it is the same contact). */
  lostAt: Map<number, number>;
}
/** An order's answer for the page's log: taken, in words said of one piece and of several ('hunts', 'hunt'); or a whole sentence (a refusal and why). */
interface Reply { ok: boolean; u: ConvoyUnit; does?: [string, string]; says?: string }

interface Merchant { u: ConvoyUnit; dx: number; dy: number; armed: boolean; nextGun: number; lost: boolean }
interface Torp { by: number; at: number; aims: number[]; escort: boolean; x: number; y: number }
interface Burst { at: number; by: number; on: number; x: number; y: number; ahead: boolean; group: boolean; kit: ConvoyKit }
interface Later { at: number; run: () => void }

const live = (u: ConvoyUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
/** What an escort is about while she hunts a boat. */
const HUNTING = new Set<EscortDoing>(['run', 'search', 'attack', 'open']);

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
  readonly tally: ConvoyTally = { attacks: 0, hunts: 0, salvoes: 0, hits: 0, forcedUp: 0, kept: 0, escaped: 0, scuttled: 0, rammed: 0, shelled: 0, crushed: 0, captured: 0 };
  /** Battle seconds each side was commanded by its player rather than the staff (0: the staff had it all night). */
  readonly steered: Record<Side, number> = { att: 0, def: 0 };

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
  /** Whether the convoy has been told it is under attack (the first salvo fired, not an escort's first sighting). */
  private attackSaid = false;
  /** The boats each side's pieces know where they are now (the player sees only his own side's contacts, ruling 241 Q6). */
  private readonly known: Record<Side, Set<number>> = { att: new Set(), def: new Set() };
  /** Where and when each side last knew each piece of the other's (kept a while once lost, LAST_SEEN). */
  private readonly spots: Record<Side, Map<number, ConvoySpot>> = { att: new Map(), def: new Map() };
  /** The battle second any escort last held each boat: a contact on a boat none held for AGAIN is news to the side. */
  private readonly heldAt = new Map<number, number>();
  /** The boats whose contact the side was told of and not yet told lost; the second the side last lost each (no escort holding her), and the line of it still to be told. */
  private readonly reported = new Set<number>();
  private readonly goneAt = new Map<number, number>();
  private readonly losing = new Map<number, () => void>();
  /** The battle second each gun last fired at each piece ('<gun>:<target>'): one that opens fire again within AGAIN says nothing new. */
  private readonly firedAt = new Map<string, number>();
  /** The live merchants of the outermost column on each side (-1 west, 1 east), kept until a merchant is lost. */
  private outerCols: Map<number, Merchant[]> | null = null;
  private handed = false;
  private handedAt = -1;
  private staffPart: { counts: ConvoyCounts; tally: ConvoyTally } | undefined;
  /** The rest of the night as the staff would settle it, from the computer's own nights (calibrate.ts), for 'dice': its counts, and its measures' means to split them by. */
  rest: ((sec: number) => ConvoyCounts & { tally?: Partial<ConvoyTally> }) | null = null;

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
          holds: -1, echo: null, seenAt: -1, huntFrom: -1, dx: 0, dy: 0, openTo: 0, nextGun: 0, nextShell: 0, harm: 0, order: null, target: -1, tx: 0, ty: 0, until: 0, attacks: 0,
          chase: -1, prey: -1, lostAt: new Map() };
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
          side: Math.sin(from) >= 0 ? 1 : -1, ahead: this.rng.range(-0.3, 0.7), until: 0, lastHeard: -Infinity, sees: new Set(), order: null, target: -1, depth: null,
          abandoned: false, saying: null, ducking: false, duckQuiet: false, duckUp: -Infinity, off: false };
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
    for (const side of ['att', 'def'] as const) if (!this.ai[side] && this.ticket[side].human) this.steered[side] += dt;
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
    const pack = this.packLeft();
    if (now >= this.limit || !pack) this.finish(now >= this.limit ? 'time' : 'withdrew');
  }

  /** Whether any boat of the pack is still in the night or still to come up: with none, the night is over. */
  private packLeft(): boolean { return this.boats.some((b) => b.doing !== 'left' && b.doing !== 'lost' && b.arrive !== Infinity); }

  /** Whether the pack came and is gone: every boat that came up has left or been lost, and none is still to come. */
  private packGone(): boolean { return !this.packLeft() && this.boats.some((b) => b.doing !== 'out'); }

  /**
   * While no boat is up with the convoy and nothing is in the water, the convoy and its screen only steam on: the clock
   * runs on to a second before the next boat comes up (no roll is thrown in such seconds, so nothing moves but time).
   */
  private quiet(): void {
    // (nor once the pack is gone: the night ends at the tick, not at dawn)
    if (this.torps.length || this.bursts.length || this.later.length || this.packGone()) return;
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
    // (the seconds run on count as commanded by whoever commanded each side, as the ticks' own do)
    for (const side of ['att', 'def'] as const) if (!this.ai[side] && this.ticket[side].human) this.steered[side] += gap;
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

  /**
   * A boat's depth changed. 'reason' is her captain's, in plain words, where he took her down himself; where that goes
   * against what her commander ordered her to do (attack on the surface, fire her deck gun, keep at periscope depth),
   * the event says so ('override'), and the order stands for when the danger has passed. It is said by her keel and
   * where she is going: going down ('dives', 'goes deep') at once, coming up ('surfaces', 'comes up to periscope
   * depth') once her keel is there (boatHelm), so that no line has her up before she is.
   *
   * A duck ('duck': her captain takes her to periscope depth from the surface for an escort that found her or is
   * closing, and up again once free) is told in words only where it is news: not within AGAIN of her coming back up from
   * her last duck (on a radar night she may duck every minute while the escorts' radar keeps finding her). Its coming
   * up is told where its going down was, and a turn back down before she was up is no line at all: the log never has
   * her up when it said nothing of it. The scene's 'sea:dive' goes out all the same.
   */
  private setDive(b: Boat, dive: Dive, why: string, reason?: string, duck = false): boolean {
    if (b.dive === dive) {
      // (forced up while already coming up to the surface: the line she will say there is the forced one)
      if (why === 'forced' && b.saying) b.saying = { ...b.saying, why, reason: undefined, override: false, quiet: false };
      return false;
    }
    // (a duck that turns her back down before her rise to the surface was said: she is still down as far as the log goes)
    const back = duck && !!b.saying?.rising && b.saying.dive === 'surfaced' && b.keel < B.scope;
    b.dive = dive;
    b.keelTo = dive === 'surfaced' ? 0 : dive === 'periscope' ? B.scope : B.deep;
    if (dive !== 'surfaced' && b.under === 0) b.under = this.sec;
    if (dive === 'surfaced') b.under = 0;
    const override = !!reason && (b.order === 'attack' || b.order === 'gun' || (b.order === 'dive' && dive === 'deep'));
    const rising = b.keelTo < b.keel;
    let quiet = false;
    if (back) { quiet = true; b.ducking = true; }
    else if (duck && !rising) { b.ducking = true; quiet = b.duckQuiet = this.sec - b.duckUp < AGAIN; }
    else if (dive === 'surfaced') quiet = why === 'attack' && b.ducking && b.duckQuiet;
    else b.ducking = b.duckQuiet = false;
    b.saying = { dive, why, reason, override, rising, quiet };
    if (!rising) this.sayDive(b);
    return true;
  }

  /** The line of a boat's change of depth (the scene's 'sea:dive'), said when it is true: at once going down, once there coming up. */
  private sayDive(b: Boat): void {
    const s = b.saying;
    if (!s) return;
    b.saying = null;
    const n = b.u.name;
    const words = s.dive === 'surfaced' ? (s.why === 'forced' ? `${n} is forced to the surface` : `${n} surfaces`)
      : s.dive === 'deep' ? `${n} goes deep` : s.rising ? `${n} comes up to periscope depth` : b.keel < b.keelTo ? `${n} dives` : `${n} stays at periscope depth`;
    // (up from a duck: the second it ended, from which the next is no news for AGAIN)
    if (s.dive === 'surfaced') { if (b.ducking) b.duckUp = this.sec; b.ducking = b.duckQuiet = false; }
    this.say('sea:dive', b.u, s.quiet ? '' : `${words}${s.reason ? `: ${s.reason}` : ''}.`, { dive: s.dive, keel: b.keelTo, why: s.why, ...(s.reason ? { reason: s.reason } : {}), ...(s.override ? { override: true } : {}) });
  }

  /** Whether she is on the surface, keel and all: only there do radar and lookouts find her, her deck gun fire, or a gun hit her. */
  private atSurface(b: Boat): boolean { return b.dive === 'surfaced' && b.keel <= 0; }

  /** Whether her lookouts (or her periscope) have an escort in sight now. */
  private seesEscort(b: Boat): boolean { return !!this.seenEscort(b); }

  /** The nearest escort her lookouts (or her periscope) have in sight now, or none. */
  private seenEscort(b: Boat): Escort | null {
    let best: Escort | null = null, bd = Infinity;
    for (const id of b.sees) { const e = this.escortOf.get(id); if (!e || e.doing === 'lost') continue; const d = dist(e.u.x, e.u.y, b.u.x, b.u.y); if (d < bd) { bd = d; best = e; } }
    return best;
  }

  /**
   * Whether she is hunted, and so why her captain takes her deep (the rules owner, round 3, (ii)): an escort holds her
   * by sonar, depth charges are falling round her, or an escort after her is running in on her within its attack run
   * (closing within its sonar's reach, E.asdic). Found by radar or lookouts with none of these, she is not hunted.
   */
  private hunted(b: Boat): string | null {
    const id = b.u.id;
    if (this.bursts.some((x) => x.on === id)) return 'depth charges are falling round her';
    for (const e of this.escorts) if (e.doing !== 'lost' && e.holds === id && e.echo?.kind === 'asdic') return 'an escort has found her';
    for (const e of this.escorts) {
      if ((e.doing !== 'run' && e.doing !== 'attack') || (e.holds !== id && e.prey !== id && e.chase !== id)) continue;
      if (dist(e.u.x, e.u.y, b.u.x, b.u.y) <= E.asdic && Math.abs(turn(e.u.face, bearing(e.u.x, e.u.y, b.u.x, b.u.y))) < 0.6) return 'an escort is running in on her';
    }
    return null;
  }

  /** Her captain takes her deep because she is hunted: she lies deep and quiet until the long quiet has passed (E.lostFor). */
  private goDeep(b: Boat, reason: string): void {
    b.doing = 'deep';
    b.until = this.sec + 300;
    // (the hunt is what she hears: the quiet she waits out runs from now)
    b.lastHeard = this.sec;
    this.setDive(b, 'deep', 'evade', reason);
  }

  /** At periscope depth with no escort holding her and none in her sight: free to come up again. */
  private free(b: Boat, heard: number): boolean { return b.keel >= B.scope - 0.5 && heard < 0 && !this.seesEscort(b); }

  private boatHelm(b: Boat, dt: number, checking: boolean): void {
    const u = b.u, now = this.sec;
    // Keel toward her depth (a look: a crash dive to periscope depth in diveTime, deeper at a metre a second, up at RISE).
    const rate = b.keelTo > b.keel ? (b.keel < B.scope ? B.scope / B.diveTime : 1) : RISE;
    b.keel = b.keel < b.keelTo ? Math.min(b.keelTo, b.keel + rate * dt) : Math.max(b.keelTo, b.keel - rate * dt);
    if (b.saying && b.keel === b.keelTo) this.sayDive(b);
    if (b.tubes < B.tubes && b.load > 0 && now >= b.reloadAt) { const k = Math.min(B.tubes - b.tubes, b.load); b.tubes += k; b.load -= k; b.reloadAt = b.load > 0 && b.tubes < B.tubes ? now + B.reload : Infinity; }
    const heard = this.firstHeard(b);
    if (heard >= 0) b.lastHeard = now;
    // (on the surface's speeds only once she is there: a boat coming up makes her dived speed until her keel is out)
    const up = b.dive === 'surfaced' && b.keel <= 1;
    let goX = u.x, goY = u.y, speed = 0;
    b.off = false;
    switch (b.doing) {
      case 'close': {
        if (this.boatWantsDown(b, heard)) break;
        const under = b.order === 'under';
        const [gx, gy] = this.station(b);
        const far = dist(u.x, u.y, gx, gy);
        const across = this.acrossTrack(b);
        if (under && far < 5000 && b.dive === 'surfaced' && across.reach) this.setDive(b, 'periscope', 'attack');
        goX = gx; goY = gy;
        let off: Escort | null = null;
        if (b.dive !== 'surfaced' && b.order === null) {
          // (her captain's own: down for being found, she keeps closing at periscope depth; hunted, she goes deep; free
          // again, at periscope depth with no escort holding her and none in her sight, she comes up. Unheard but kept
          // down by an escort in her sight where she cannot close dived, far off her station or unable to get within reach
          // of a ship dived within DIVED_IN, she draws off out of its sight, to come up and run round on the surface: she
          // never comes up in an escort's sight, and never creeps in astern)
          const hunt = this.hunted(b);
          if (hunt) { this.goDeep(b, hunt); break; }
          if (this.free(b, heard)) this.setDive(b, 'surfaced', 'attack');
          else if (heard < 0 && now - b.lastHeard > CONVOY.check * 20 && (far > SURFACE_RUN || !across.reach)) off = this.seenEscort(b);
        } else if (b.dive !== 'surfaced' && (b.order === 'attack' || b.order === 'gun')) {
          // (forced down against an order to attack: she keeps at periscope depth, attacks dived if the columns come
          // within her reach, and comes up to press in on the surface once no escort is in her sight)
          if (b.keel >= B.scope - 0.5 && !this.seesEscort(b)) this.setDive(b, 'surfaced', 'attack');
        } else if (b.dive !== 'surfaced' && under && heard < 0 && now - b.lastHeard > CONVOY.check * 20
          && (far > SURFACE_RUN || (!across.reach && (now - b.under > DIVED_IN || !this.escorts.some((e) => e.doing !== 'lost' && dist(e.u.x, e.u.y, u.x, u.y) < E.radar))))) {
          // (ordered to attack submerged, dived and unheard: she comes up to run on the surface far off, or where she cannot
          // get in dived within DIVED_IN and no escort is near enough to find her there, or she has been down DIVED_IN
          // already; said why, since the order was to go in submerged)
          this.setDive(b, 'surfaced', 'attack', far > SURFACE_RUN ? 'too far off to get in submerged, she runs in on the surface' : 'unable to get within reach submerged, she runs round on the surface');
        }
        // (dived she is barely faster than the convoy: she closes across its track, and the columns come to her)
        if (off) { const a = bearing(off.u.x, off.u.y, u.x, u.y); goX = u.x + Math.sin(a) * 1000; goY = u.y + Math.cos(a) * 1000; b.off = true; }
        else if (b.dive !== 'surfaced') { goX = across.x; goY = across.y; }
        speed = !up ? B.dived : far > 4000 ? B.surfaced : B.cruise;
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
        speed = up ? B.surfaced : B.dived;
        if (this.boatWantsDown(b, heard)) break;
        // (down on her way out, as closing: hunted she goes deep, free she comes up)
        if (b.dive !== 'surfaced' && b.order === null) {
          const hunt = this.hunted(b);
          if (hunt) { this.goDeep(b, hunt); break; }
          if (this.free(b, heard)) this.setDive(b, 'surfaced', 'attack');
        } else if (b.dive !== 'surfaced' && b.order === 'gun' && b.keel >= B.scope - 0.5 && !this.seesEscort(b)) this.setDive(b, 'surfaced', 'attack');
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
        // (found again below, by sonar: the radar or lookouts' contact that sent her down ended as she went under, and is no reason to go deep)
        if (b.doing === 'evade' && this.escorts.some((e) => e.holds === b.u.id && e.echo?.kind === 'asdic' && e.doing !== 'lost')) { b.doing = 'deep'; this.setDive(b, 'deep', 'evade', 'an escort has found her'); }
        // (deep and still hunted, though no escort holds her: the charges falling or an escort running in restart her wait)
        if (b.doing === 'deep' && heard < 0 && this.hunted(b)) b.lastHeard = now;
        const quiet = now - b.lastHeard;
        // (ordered to periscope depth and taken deep by her captain: she comes back up to it once the danger has passed,
        // after the same quiet as a boat whose captain took her down against an order to attack)
        if (b.order === 'dive' && b.dive === 'deep' && quiet >= E.lostFor && now >= b.until) { this.setDive(b, 'periscope', 'ordered'); b.doing = 'evade'; }
        if (b.order === 'dive' || b.order === 'deep') break;
        // (her captain's own evasion, nothing hunting near her: she goes in at periscope depth once near enough)
        if (b.doing === 'evade' && (b.order === null || b.order === 'evade') && heard < 0 && quiet >= PRESS && now >= b.until && b.tubes > 0 && b.harm < 0.5
          && dist(u.x, u.y, ...this.station(b)) < PRESS_AT && this.acrossTrack(b).reach && !this.escorts.some((e) => e.doing !== 'screen' && e.doing !== 'back' && e.doing !== 'lost' && dist(e.u.x, e.u.y, u.x, u.y) < E.asdic * 1.5)) {
          b.doing = 'close'; b.order = null;
          break;
        }
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

  /**
   * A boat on the surface (or coming up to it) that sees an escort closing on her, or is held, goes under (her captain's
   * call). Hunted (the rules owner, round 3, (ii): hunted), she goes deep; else she dives only to periscope depth and
   * keeps on as she was, closing or turning away, to come up again once free (boatHelm). One her commander ordered to
   * attack on the surface or to fire her deck gun stays up while she is only held, and goes under only for an escort
   * she sees closing on her: forced down, with the reason said, she keeps at periscope depth and the order stands. One
   * ordered to dive or evade does as ordered.
   */
  private boatWantsDown(b: Boat, heard: number): boolean {
    if (b.dive !== 'surfaced') return false;
    if (b.order === 'surface') return false;
    const u = b.u;
    const stay = b.order === 'attack' || b.order === 'gun';
    let why: string | null = null, reason: string | undefined;
    if (heard >= 0 && !stay) { why = this.escortOf.get(heard)!.echo?.kind === 'radar' ? 'radar' : 'evade'; reason = 'an escort has found her'; }
    if (!why) {
      for (const id of b.sees) {
        const e = this.escortOf.get(id);
        if (!e || e.doing === 'lost') continue;
        const d = dist(e.u.x, e.u.y, u.x, u.y);
        const closing = Math.abs(turn(e.u.face, bearing(e.u.x, e.u.y, u.x, u.y))) < 0.6 && e.u.speed > E.screen * 0.9;
        if (d < this.ticket.place.light.vis * B.lookout * 0.8 && closing) { why = 'evade'; reason = 'an escort is closing'; break; }
      }
    }
    if (b.order === 'dive' || b.order === 'deep') { why = 'ordered'; reason = undefined; }
    if (!why) return false;
    if (why === 'ordered' || b.order === 'evade') {
      this.setDive(b, b.order === 'deep' ? 'deep' : 'periscope', why, reason);
      b.doing = 'evade';
      b.until = this.sec + 300;
      return true;
    }
    const hunt = b.order === null ? this.hunted(b) : null;
    if (hunt) { this.goDeep(b, hunt); return true; }
    this.setDive(b, 'periscope', why, reason, true);
    if (b.doing !== 'close' && b.doing !== 'away') b.doing = 'close';
    return true;
  }

  private leave(b: Boat, why: 'spent' | 'hurt' | 'ordered'): void {
    if (b.doing === 'left' || b.doing === 'lost') return;
    b.doing = 'left';
    b.u.state = 'withdrawn';
    b.u.act = 'idle';
    b.u.extra.gun = undefined;
    this.dropContacts(b.u.id);
    this.say('withdrawn', b.u, why === 'spent' ? `${b.u.name} has fired her last torpedo and turns for home.` : why === 'hurt' ? `${b.u.name} slips away, damaged.` : `${b.u.name} breaks off.`, { why });
  }

  /** The live merchants of the outermost column on a side of the convoy as it stands now (-1 west, 1 east): the columns thin as ships are sunk. */
  private outerColumn(side: number): Merchant[] {
    if (!this.outerCols) {
      this.outerCols = new Map();
      for (const k of [-1, 1]) {
        let edge = -Infinity;
        for (const m of this.merchants) if (!m.lost && k * m.dx > edge + 1) edge = k * m.dx;
        this.outerCols.set(k, this.merchants.filter((m) => !m.lost && Math.abs(k * m.dx - edge) <= 1));
      }
    }
    return this.outerCols.get(side) ?? [];
  }

  /**
   * Where a boat closing to attack makes for: firing reach off the ship she was ordered to attack, or else off the
   * outermost ship on her side of the convoy as it stands now (the one in that column nearest her place along it). One
   * ordered to attack submerged makes on the surface for a place ahead of it, to dive there and let the columns come on.
   */
  private station(b: Boat): [number, number] {
    const u = b.u;
    const t = (b.order === 'attack' || b.order === 'under') && b.target >= 0 ? this.byId.get(b.target) : undefined;
    if (t && live(t)) {
      if (b.order === 'under') return [t.x + (u.x >= t.x ? 1 : -1) * B.fireAt * 0.8, t.y + UNDER_AHEAD];
      const d = Math.max(1, dist(u.x, u.y, t.x, t.y)), r = Math.min(d, B.fireAt * 0.8);
      return [t.x + ((u.x - t.x) / d) * r, t.y + ((u.y - t.y) / d) * r];
    }
    const col = this.outerColumn(b.side);
    if (!col.length) return [this.cx + b.side * (this.halfW + B.fireAt * 0.9), this.cy + b.ahead * this.halfL + 400];
    const want = b.ahead * this.halfL;
    let m = col[0];
    for (const o of col) if (Math.abs(o.dy - want) < Math.abs(m.dy - want)) m = o;
    return [m.u.x + b.side * B.fireAt * 0.9, m.u.y + (b.order === 'under' ? UNDER_AHEAD : 400)];
  }

  /**
   * A dived boat's way in. She is barely faster than the convoy (3.9 m/s to 3.6): seen from the convoy she closes
   * fast on a ship abeam or astern of her and only crawls up on one ahead, so she makes for the ship she can get within
   * firing reach of soonest (the one she was ordered to attack, else any), on the heading that gets her there soonest,
   * and the columns come down on a boat lying ahead of them. 'reach': whether she can get there within DIVED_IN; a
   * boat that cannot would only trail the convoy, and must first run round on the surface.
   */
  private acrossTrack(b: Boat): { x: number; y: number; reach: boolean } {
    const u = b.u, v = CONVOY.convoy.speed, V = B.dived, r = B.fireAt * 0.9;
    const t = (b.order === 'attack' || b.order === 'under') && b.target >= 0 ? this.byId.get(b.target) : undefined;
    const ships = t && live(t) ? [t] : this.merchants.filter((m) => !m.lost).map((m) => m.u);
    let best = Infinity, hx = 0, hy = v;
    for (const m of ships) {
      const dx = m.x - u.x, dy = m.y - u.y, d = Math.hypot(dx, dy);
      if (d <= r) { best = 0; hx = 0; hy = 0; break; }
      // (her speed toward the ship as the convoy sees it, on the heading that holds that bearing: the convoy's way added back)
      const sin = dy / d, s = -v * sin + Math.sqrt(v * v * sin * sin + V * V - v * v);
      const time = (d - r) / Math.max(1e-6, s);
      if (time < best) { best = time; hx = (dx / d) * s; hy = (dy / d) * s + v; }
    }
    return { x: u.x + hx * 60, y: u.y + hy * 60, reach: best <= DIVED_IN };
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
    this.noteContact('torpedo');
    // (with the salvo's running time: the screens hold their slow-down until its torpedoes are home)
    if (!this.attackSaid) { this.attackSaid = true; this.events.push({ t: this.sec, what: 'sea:attack', text: 'The convoy is under attack.', extra: { by: b.u.id, running } }); }
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
    this.outerCols = null;
    this.counts.mv++;
    this.say('sea:torpedoHit', m.u, `A torpedo hits ${m.u.name}.`, { from: by }, 'torpedo_hit', 'att');
    // (her crew leave her at once, as the scene reads it: out of the fight from the hit, her wreck there for her men)
    this.say('sea:abandon', m.u, '', { ship: 1 }, undefined, 'def');
    const how = ['settle', 'break', 'burn', 'capsize'][Math.floor(mix(this.ticket.seed ^ (m.u.id * 104729)) * 4)];
    const u = m.u;
    u.state = 'gone';
    u.figures = 0; u.str = 0;
    u.extra.ships = 0; u.extra.sunk = 1; u.extra.gun = undefined;
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
    if (target && (target.doing === 'lost' || target.doing === 'left')) this.drop(e, 'ended');
    const huntCap = groupsKit ? E.groupFor : E.huntFor;
    // Sent after a boat she does not hold: her datum follows the boat while another of the side holds her.
    if (e.chase >= 0 && e.holds < 0 && this.known.def.has(e.chase)) { const k = this.boatOf.get(e.chase)!; e.dx = k.u.x; e.dy = k.u.y; e.seenAt = now; }
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
        if (!b) {
          // (sent after a boat she does not hold: at full speed to where her side last knew the boat, then search there)
          goX = e.dx; goY = e.dy; speed = E.top;
          if (dist(u.x, u.y, goX, goY) < 600) { e.doing = 'search'; e.seenAt = now; }
          if (now - e.huntFrom > huntCap) this.goBack(e);
          break;
        }
        goX = e.echo && e.echo.kind !== 'asdic' && b.dive === 'surfaced' ? b.u.x : e.dx; goY = e.echo && e.echo.kind !== 'asdic' && b.dive === 'surfaced' ? b.u.y : e.dy;
        speed = E.top;
        const d = dist(u.x, u.y, goX, goY);
        if ((b.doing === 'up' || b.doing === 'gun') && this.atSurface(b) && !b.abandoned && d <= E.ram && checking && this.staffRuns(u) && b.struck < 0 && this.rng.chance(S.ramShare)) { this.ram(e, b); break; }
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
    e.prey = b.u.id;
    this.tally.hunts++;
    // A group trained to hunt sends a second ship to work the contact with the first (escortgroups): the creeping attack.
    if (by === 'staff' && e.kit.groups && this.staffRuns(e.u)) {
      const mate = this.escorts.filter((o) => o !== e && o.doing === 'screen' && o.kit.groups && o.holds < 0)
        .sort((a, c) => dist(a.u.x, a.u.y, b.u.x, b.u.y) - dist(c.u.x, c.u.y, b.u.x, b.u.y) || a.u.id - c.u.id)[0];
      if (mate && this.awayCount() < Math.ceil(E.keepAway * this.escorts.length)) {
        mate.holds = b.u.id; mate.echo = e.echo ? { ...e.echo } : null; mate.prey = b.u.id; mate.doing = 'search'; mate.huntFrom = this.sec; mate.seenAt = this.sec; mate.dx = b.u.x; mate.dy = b.u.y; mate.attacks = 0;
      }
    }
  }

  private goBack(e: Escort): void {
    if (e.holds >= 0) this.drop(e, 'left');
    e.doing = 'back';
    e.order = null;
    e.chase = -1;
    e.prey = -1;
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
      // (a boat already blown to the surface is out of the pattern set for her below: one boat is forced up once)
      if (b.doing === 'up' || b.doing === 'gun') continue;
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

  /**
   * Forced to the surface: doomed (her crew will give her up), or hurt and trying to make good her damage. Her keel
   * takes a while to come up (RISE): her crew give her up, abandon her and take to the water only once she is there.
   */
  private forcedUp(b: Boat, doomed: boolean, by: number): void {
    if (b.doing === 'up' || b.doing === 'gun') return;
    this.tally.forcedUp++;
    b.doing = 'up';
    b.doomed = doomed;
    this.setDive(b, 'surfaced', 'forced');
    b.repairAt = doomed ? Infinity : this.sec + S.repair;
    b.u.act = 'idle';
    const near = this.nearestEscort(b.u.x, b.u.y);
    const close = near ? dist(near.u.x, near.u.y, b.u.x, b.u.y) : Infinity;
    if (!doomed) return;
    const rise = Math.ceil(b.keel / RISE);
    // Her crew's call: fight with the deck gun while she floats, strike to an escort (subSurrender), or abandon her.
    if (this.ticket.place.surrender && near && close <= 1500 && this.rng.chance(S.strike)) {
      b.struck = near.u.id;
      const to = near.u.id;
      this.later.push({ at: this.sec + rise, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.crewLeave(b); this.say('sea:surrender', b.u, `${b.u.name}'s crew give her up.`, { ship: 1, how: 'scuttle', by: to }, undefined, 'att'); } });
    } else if (near && close <= S.gunAt && this.rng.chance(S.fight)) {
      b.doing = 'gun'; b.gunOn = near.u.id;
    }
    const [lo, hi] = S.scuttleIn;
    b.scuttleAt = this.sec + rise + lo + Math.round(this.rng.range(0, hi - lo));
    const at = b.scuttleAt;
    this.later.push({ at: at - 50, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.say('sea:abandon', b.u, `${b.u.name}'s crew abandon her.`, { ship: 1 }, undefined, 'att'); this.crewLeave(b); } });
    this.later.push({ at: at - 45, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.say('sea:men', b.u, '', { ship: 1, men: CONVOY.crew.ss, dead: Math.round(CONVOY.crew.ss * mix(b.u.id * 17 + this.ticket.seed) * 0.4) }, undefined, 'att'); } });
    this.later.push({ at, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.boatLost(b, b.struck >= 0 && this.boarded(b) && this.rng.chance(S.taken) ? 'captured' : 'scuttled', by); } });
  }

  /**
   * Her crew leave her (abandoned, or given up): she is out of the fight from that moment, as the scene reads it: no
   * ship of her fighting, her gun silent and no one's target. She is sunk (sunk 1) only when she goes down (boatLost).
   */
  private crewLeave(b: Boat): void {
    b.abandoned = true;
    b.gunOn = -1;
    b.doing = 'up';
    const u = b.u;
    u.extra.ships = 0; u.figures = 0; u.str = 0; u.extra.gun = undefined;
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
      if (e.holds >= 0) this.drop(e, 'ended');
      u.state = 'gone'; u.speed = 0;
    } else {
      e.harm = 0;
    }
  }

  // ---- Guns ---------------------------------------------------------------------------------------------------------

  private guns(): void {
    // A boat on the surface fighting with her deck gun, or ordered to: on the surface, keel and all, and her crew aboard.
    for (const b of this.boats) {
      if (b.doing === 'lost' || b.doing === 'left' || b.doing === 'out') continue;
      if (!this.atSurface(b) || b.abandoned) { b.u.extra.gun = undefined; continue; }
      let on = b.doing === 'gun' ? b.gunOn : b.order === 'gun' ? b.target : -1;
      const t = on >= 0 ? this.byId.get(on) : undefined;
      if (!t || !live(t) || dist(b.u.x, b.u.y, t.x, t.y) > S.gunAt) { b.u.extra.gun = undefined; continue; }
      if (b.u.extra.gun !== t.id) this.say('sea:gun', b.u, this.opens(b.u, t) ? `${b.u.name} opens fire on ${t.name} with her deck gun.` : '', { on: t.id, why: b.doing === 'gun' ? 'fight' : 'attack' }, 'deck_gun', 'att');
      b.u.extra.gun = t.id;
      this.firedAt.set(`${b.u.id}:${t.id}`, this.sec);
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
      if (!this.atSurface(b) || b.abandoned || dist(e.u.x, e.u.y, b.u.x, b.u.y) > S.gunAt || b.struck >= 0) { e.u.extra.gun = undefined; continue; }
      if (e.u.extra.gun !== b.u.id) this.say('sea:gun', e.u, this.opens(e.u, b.u) ? `${e.u.name} opens fire on ${b.u.name}.` : '', { on: b.u.id, why: 'attack' }, 'naval_gun', 'def');
      e.u.extra.gun = b.u.id;
      this.firedAt.set(`${e.u.id}:${b.u.id}`, this.sec);
      if (this.rng.chance(S.escortHits)) this.shellBoat(b, e.u.id);
    }
    for (const m of this.merchants) {
      if (m.lost || !m.armed) continue;
      let best: Boat | null = null, bd = 2000;
      for (const id of this.known.def) { const b = this.boatOf.get(id); if (!b || !this.atSurface(b) || b.abandoned || b.doing === 'lost' || b.doing === 'left') continue; const d = dist(m.u.x, m.u.y, b.u.x, b.u.y); if (d < bd) { bd = d; best = b; } }
      if (!best) { m.u.extra.gun = undefined; continue; }
      if (m.u.extra.gun !== best.u.id) this.say('sea:gun', m.u, this.opens(m.u, best.u) ? `${m.u.name}'s gunners open fire.` : '', { on: best.u.id, why: 'defend' }, 'deck_gun', 'def');
      m.u.extra.gun = best.u.id;
      this.firedAt.set(`${m.u.id}:${best.u.id}`, this.sec);
      if (this.rng.chance(S.merchantHits)) this.shellBoat(best, m.u.id);
    }
  }

  /** Whether a gun opening fire on a piece is news: not where it fired at her within AGAIN (a boat ducking under its fire and up again). */
  private opens(gun: ConvoyUnit, on: ConvoyUnit): boolean { return this.sec - (this.firedAt.get(`${gun.id}:${on.id}`) ?? -Infinity) > AGAIN; }

  private shellBoat(b: Boat, by: number): void {
    b.harm += S.escortHarm;
    // (a boat on the surface hurt past diving is kept there: no change of depth to say, so the hit says it)
    const kept = b.harm >= 1 && b.harm < 2 && b.doing !== 'up' && b.doing !== 'gun';
    this.say('sea:gunHit', b.u, `A shell hits ${b.u.name}${kept ? ': she can no longer dive' : ''}.`, { n: 1, from: by }, undefined, 'def');
    if (b.harm >= 2) { this.boatLost(b, 'shelled', by); return; }
    if (kept) { this.tally.kept++; this.forcedUp(b, false, by); }
  }

  private merchantShelled(m: Merchant, by: number): void {
    m.lost = true;
    this.outerCols = null;
    this.counts.mv++;
    const u = m.u;
    this.say('sea:abandon', u, '', { ship: 1 }, undefined, 'def');
    u.state = 'gone'; u.figures = 0; u.str = 0; u.extra.ships = 0; u.extra.sunk = 1; u.speed = 0; u.extra.gun = undefined;
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
        if (!kind) { this.drop(e, !this.atSurface(b) && e.echo?.kind !== 'asdic' ? 'deep' : 'faded'); continue; }
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

  /**
   * The sense an escort could hold this boat by, now: radar or lookouts while she is on the surface, keel and all; sonar
   * once she is under, from the moment she dives (the rules owner, round 3, (i)): a contact by radar or lookouts passes
   * to sonar only where the sonar itself can hold her, and ends where it cannot.
   */
  private senses(e: Escort, b: Boat, d: number, vis: number): Echo['kind'] | null {
    if (this.atSurface(b)) {
      if (e.kit.radar && d <= E.radar) return 'radar';
      const lit = this.zones.some((z) => z.kind === 'starshell' && dist(z.x, z.y, b.u.x, b.u.y) <= z.r);
      if (d <= vis * (lit ? 2 : 1)) return 'sight';
      return null;
    }
    if (!e.kit.asdic || d > E.asdic || e.u.speed > E.deaf) return null;
    if (Math.abs(turn(e.u.face, bearing(e.u.x, e.u.y, b.u.x, b.u.y))) > E.arc && d > E.blind) return null;
    return 'asdic';
  }

  /**
   * An escort holds a boat, by a sense, now. A contact gained is the same contact where the same escort lost the same
   * boat less than AGAIN ago ('fresh' false); it is news to the side ('news') where no escort held the boat for AGAIN,
   * with whether any escort ever held her before ('first') and how long since ('since'). Only news is reported in words:
   * a boat the side held a moment ago, picked up again by another escort or found again as she comes up from a duck, is
   * the same fight (on a radar night she may duck every minute or two). The boat's own side learns of it ('sea:found')
   * where it is news and no escort is hunting her already: a boat found again in the middle of a hunt is no news to her
   * pack. It is said in words unless she says it herself as she dives.
   */
  private hold(e: Escort, b: Boat, kind: Echo['kind'], d: number, gained: boolean): void {
    const now = this.sec;
    // (clockwise from north, 0 to under a full turn, as the scene reads a contact's bearing)
    const br = Math.round((((bearing(e.u.x, e.u.y, b.u.x, b.u.y) % TAU) + TAU) % TAU) * 1000) / 1000;
    const err = Math.round((mix(now * 131 + e.u.id * 7 + b.u.id) - 0.5) * (kind === 'asdic' ? 0.1 : 0.04) * 1000) / 1000;
    const was = e.echo?.kind;
    const fresh = gained && !(now - (e.lostAt.get(b.u.id) ?? -Infinity) < AGAIN);
    const last = this.heldAt.get(b.u.id);
    const news = gained && now - (last ?? -Infinity) >= AGAIN;
    const ago = last === undefined ? { first: true } : { first: false, since: now - last };
    const hunted = this.escorts.some((o) => o.prey === b.u.id && HUNTING.has(o.doing));
    // (news in the second her loss is to be told: the loss first, then the contact)
    if (news) { this.losing.get(b.u.id)?.(); this.reported.add(b.u.id); }
    if (HUNTING.has(e.doing)) e.prey = b.u.id;
    e.holds = b.u.id;
    e.echo = { on: b.u.id, kind, range: Math.round(d), bearing: br, err };
    e.seenAt = now;
    e.dx = b.u.x; e.dy = b.u.y;
    if (e.chase === b.u.id) e.chase = -1;
    this.known.def.add(b.u.id);
    this.heldAt.set(b.u.id, now);
    this.spots.def.set(b.u.id, { id: b.u.id, name: b.u.name, cls: 'ss', x: Math.round(b.u.x), y: Math.round(b.u.y), t: now, dive: b.dive, kind });
    if (gained || was !== kind) {
      this.noteContact('escort');
      const words = kind === 'asdic' ? `${e.u.name} has ${b.u.name} on sonar.` : kind === 'radar' ? `${e.u.name} has ${b.u.name} on radar.` : `${e.u.name} sights ${b.u.name}.`;
      this.say('sea:sonar', e.u, news ? words : '', { on: b.u.id, kind, range: Math.round(d), bearing: br, err, ...(gained ? { fresh, news, ...(news ? ago : {}) } : {}) }, undefined, 'def');
      // (a boat on the surface that will dive for it says so herself, 'dives: an escort has found her': no second line)
      const says = b.dive === 'surfaced' && (b.doing === 'close' || b.doing === 'away') && b.order !== 'attack' && b.order !== 'gun' && b.order !== 'surface' && now - b.duckUp >= AGAIN;
      if (news && !hunted) this.say('sea:found', b.u, says ? '' : `An escort has found ${b.u.name}.`, { by: kind, ...ago }, undefined, 'att');
      // Star shell over a boat seen by eye where the night is dark: a look, and it lights her for the other lookouts.
      if (kind === 'sight' && this.ticket.place.light.moon.lit < 0.6 && now >= e.nextShell) {
        e.nextShell = now + 180;
        const x = Math.round(b.u.x), y = Math.round(b.u.y);
        this.say('sea:starshell', e.u, `${e.u.name} fires star shell.`, { by: e.u.id, x, y }, 'starshell', 'def', x, y);
        this.zones.push({ id: ++this.zoneId, kind: 'starshell', x, y, r: 800, until: now + 60, side: 'def', unit: e.u.id });
      }
    }
  }

  /**
   * An escort's contact ends (the scene's 'sea:sonar' with 'lost', at once; 'gone': no escort of the side holds her now,
   * and her last known spot stays a while, LAST_SEEN). Lost ('close', 'deep', 'wake', 'faded') by the last escort that
   * held her, the loss of a contact the side was told of is told once it is sure: if no escort has found her again within
   * AGAIN ('sea:lost'). Found again sooner, by her or another, it was the same fight all along, and nothing is said either
   * way. Let go ('left', for an order or the screen) or ended with the boat's night ('ended', sunk or gone home), it
   * says nothing.
   */
  private drop(e: Escort, why: 'close' | 'deep' | 'wake' | 'faded' | 'left' | 'ended'): void {
    if (e.holds < 0) return;
    const on = e.holds, b = this.boatOf.get(on), at = this.sec;
    e.holds = -1;
    const kind = e.echo?.kind ?? 'asdic';
    e.echo = null;
    const real = why !== 'left' && why !== 'ended';
    const gone = !this.escorts.some((o) => o.holds === on);
    this.say('sea:sonar', e.u, '', { on, kind, lost: why, gone, ...(b ? { x: Math.round(b.u.x), y: Math.round(b.u.y) } : {}) }, undefined, 'def');
    // (the side had her until now: a contact on her is news only AGAIN from here, when her loss is told)
    if (real) { e.lostAt.set(on, at); this.heldAt.set(on, at); }
    if (gone && b) {
      this.goneAt.set(on, at);
      if (!real) { this.reported.delete(on); this.losing.delete(on); }
      else if (this.reported.has(on)) {
        const tell = (): void => {
          // (found again since, and held still or lost again later: the fight went on, and its own end is told)
          if (this.losing.get(on) !== tell || this.goneAt.get(on) !== at || this.escorts.some((o) => o.holds === on)) return;
          this.losing.delete(on);
          this.reported.delete(on);
          if (e.doing === 'lost' || b.doing === 'lost' || b.doing === 'left') return;
          // (told two minutes on, once sure: said with the hour she was lost, which is when the map's mark of her began)
          this.say('sea:lost', e.u, `${e.u.name} lost ${b.u.name}${why === 'deep' ? ' when she dived,' : ''} at ${clockOf(at)}, and no escort has found her again.`, { on, why, at }, undefined, 'def');
        };
        this.losing.set(on, tell);
        this.later.push({ at: at + AGAIN, run: tell });
      }
    }
    if (gone) this.known.def.delete(on);
    if (e.doing === 'attack' || e.doing === 'open' || e.doing === 'run') e.doing = 'search';
  }

  /** A boat sunk or gone from the night: every contact on her ends with nothing said, and a sunk boat leaves no last known spot. */
  private dropContacts(id: number): void {
    for (const e of this.escorts) if (e.holds === id) this.drop(e, 'ended');
    this.known.def.delete(id);
    if (this.boatOf.get(id)?.doing === 'lost') this.spots.def.delete(id);
  }

  /** The boats' lookouts: escorts within their reach are picked up in 'boat' seconds on average; lost once out of it. */
  private look(): void {
    const reach = this.ticket.place.light.vis * B.lookout;
    const p = chanceIn(D.boat, CONVOY.check);
    for (const b of this.boats) {
      if (b.doing === 'out' || b.doing === 'left' || b.doing === 'lost') continue;
      const up = b.dive !== 'deep';
      for (const e of this.escorts) {
        if (e.doing === 'lost') { b.sees.delete(e.u.id); continue; }
        const d = dist(b.u.x, b.u.y, e.u.x, e.u.y);
        if (!up || d > reach * 1.2) { b.sees.delete(e.u.id); continue; }
        if (b.sees.has(e.u.id) || d > reach) continue;
        if (this.rng.chance(p)) b.sees.add(e.u.id);
      }
    }
    this.known.att.clear();
    for (const b of this.boats) if (b.doing !== 'out' && b.doing !== 'left' && b.doing !== 'lost') for (const id of b.sees) this.known.att.add(id);
    for (const id of this.known.att) { const u = this.byId.get(id)!; this.spots.att.set(id, { id, name: u.name, cls: u.kind, x: Math.round(u.x), y: Math.round(u.y), t: this.sec }); }
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

  /**
   * The night's first contact between the two sides, by an escort's contact or a boat's first salvo: the screens run the
   * clock fast until it. It says nothing itself: the escort's report, or the convoy under attack at the first salvo
   * ('sea:attack', fire), says what it was.
   */
  private noteContact(by: 'escort' | 'torpedo'): void { if (this.contactAt === null) { this.contactAt = this.sec; this.events.push({ t: this.sec, what: 'contact', text: '', extra: { by } }); } }

  private say(what: FieldEvent['what'], u: ConvoyUnit, text: string, extra: Record<string, unknown>, cue?: string, side?: Side, x?: number, y?: number): void {
    const e: FieldEvent = { t: this.sec, what, unit: u.id, side: side ?? u.side, text, extra };
    if (cue) e.cue = cue;
    if (x !== undefined && y !== undefined) { e.x = x; e.y = y; }
    this.events.push(e);
  }

  /** What the scene reads off each piece each view (a boat's depth and hunters, an escort's echo and hunt), and what the page reads of what each is about. */
  private showLooks(): void {
    for (const b of this.boats) {
      const x = b.u.extra;
      x.dive = b.dive; x.keel = Math.round(b.keel * 10) / 10; x.scope = b.dive === 'periscope' && b.keel <= B.scope + 0.5; x.tubes = b.tubes;
      x.heard = this.heardBy(b);
      x.doing = b.doing; x.order = b.order ?? undefined; x.off = b.off || undefined;
      // (the ship her order names, for the page to draw the order: an attack's or the deck gun's)
      x.aim = (b.order === 'attack' || b.order === 'under' || b.order === 'gun') && b.target >= 0 ? b.target : undefined;
      // (down of her captain's own accord: the second she comes back up if nothing hears her before it, boatHelm's quiet)
      x.upAt = (b.doing === 'evade' || b.doing === 'deep') && b.order !== 'dive' && b.order !== 'deep' && b.harm < 0.5 && b.tubes + b.load > 0 ? Math.max(b.lastHeard + E.lostFor, b.until)
        : b.order === 'dive' && b.dive === 'deep' ? Math.max(b.lastHeard + E.lostFor, b.until) : undefined;
      // (forced up too badly hurt to save: her crew will give her up, whatever they try first)
      x.doomed = (b.doing === 'up' || b.doing === 'gun') && b.doomed ? true : undefined;
      if (b.doing === 'up' || b.doing === 'gun') b.u.act = 'idle';
    }
    for (const e of this.escorts) {
      const x = e.u.extra;
      x.echo = e.echo ?? undefined;
      x.huntPhase = e.doing === 'search' ? 'search' : e.doing === 'run' || e.doing === 'attack' ? (e.echo && e.echo.kind === 'asdic' && e.echo.range <= E.blind ? 'blind' : 'run') : e.doing === 'open' ? 'open' : undefined;
      x.huntUntil = x.huntPhase ? e.huntFrom + (e.kit.groups ? E.groupFor : E.huntFor) : undefined;
      x.doing = e.doing; x.chase = e.chase >= 0 ? e.chase : undefined; x.prey = e.prey >= 0 ? e.prey : undefined; x.order = e.order ?? undefined;
      // (where she makes for and holds nothing to steer by: the spot given, the wreck, where her hunt last knew the boat)
      x.goal = e.doing === 'move' || e.doing === 'rescue' ? { x: Math.round(e.tx), y: Math.round(e.ty) }
        : e.holds < 0 && (e.doing === 'run' || e.doing === 'search') ? { x: Math.round(e.dx), y: Math.round(e.dy) } : undefined;
      // (the second she gives up and goes back to the screen, as escortHelm has it: her hunt's time, or a search's without contact)
      const cap = e.kit.groups ? E.groupFor : E.huntFor;
      x.giveUp = HUNTING.has(e.doing) ? e.huntFrom + cap : undefined;
      // (a search gives up after its quarter hour without contact only while she holds nothing: holding the boat, the clock is her hunt's own)
      if (e.doing === 'search' && e.holds < 0 && x.giveUp !== undefined) x.giveUp = Math.min(x.giveUp as number, e.seenAt + E.lostFor);
    }
  }

  // ---- Orders -------------------------------------------------------------------------------------------------------

  /**
   * One of the player's orders. Every piece of his it names answers, told to his side as 'sea:order' for the log: the
   * order taken (pieces that take it in the same words in one line, 'Escorts 2 and 5 hunt Boat 3.'), or refused and
   * why. An order whose own doing is told already (a dive, star shell, breaking off) is taken with no words of its own.
   */
  command(c: Command | ConvoyCommand, by: Side): boolean {
    if (this.over) return false;
    if (c.do === 'auto') { this.ai[c.side] = c.on; return true; }
    if (c.do === 'dice') { this.handToStaff(); return true; }
    const units = 'units' in c && Array.isArray(c.units) ? (c.units as number[]) : [];
    const replies: Reply[] = [];
    for (const id of units) {
      const u = this.byId.get(id);
      if (!u || u.side !== by || u.kind === 'mv' || (this.ticket.place.mine !== undefined && u.nat !== this.ticket.place.mine)) continue;
      const she = u.kind === 'ss' ? 'she' : 'it';
      if (!live(u)) {
        const why = u.state === 'gone' ? `${she} has been sunk` : this.boatOf.get(id)?.doing === 'out' ? 'she has not come up with the convoy yet' : 'she has left the night';
        replies.push({ ok: false, u, says: `${u.name} can't take orders: ${why}.` });
        continue;
      }
      replies.push(u.kind === 'dd' ? this.orderEscort(this.escortOf.get(id)!, c) : this.orderBoat(this.boatOf.get(id)!, c));
    }
    this.answer(by, c.do, replies);
    // (the last boat sent home: the night ends there and then, as it does when she goes home by herself)
    if (this.packGone()) this.finish('withdrew');
    return replies.some((r) => r.ok);
  }

  /** Tells the side what came of one order: the pieces that took it in the same words in one line, each refusal on its own. */
  private answer(by: Side, what: string, replies: Reply[]): void {
    const done = new Set<Reply>();
    for (const r of replies) {
      if (done.has(r)) continue;
      const group = r.ok && r.does ? replies.filter((o) => o.ok && o.does && o.does[0] === r.does![0]) : [r];
      for (const o of group) done.add(o);
      const text = r.does ? `${namesOf(group.map((o) => o.u.name))} ${group.length === 1 ? r.does[0] : r.does[1]}.` : r.says ?? '';
      if (!text && !r.ok) continue;
      this.say('sea:order', r.u, text, { ok: r.ok, do: what, units: group.map((o) => o.u.id) }, undefined, by);
    }
  }

  private orderEscort(e: Escort, c: Command | ConvoyCommand): Reply {
    const u = e.u, no = (why: string): Reply => ({ ok: false, u, says: `${u.name} can't ${why}.` });
    if (e.doing === 'lost') return no('take orders: it has been sunk');
    switch (c.do) {
      case 'convoy:hunt': case 'attack': {
        // A boat the side holds now, or one it lost touch with lately: the hunt goes to her last known spot.
        const b = this.boatOf.get(c.target);
        if (!b) return no('hunt that: it is not a boat');
        if (b.doing === 'lost') return no(`hunt ${b.u.name}: she has been sunk`);
        const held = this.known.def.has(b.u.id), spot = this.spots.def.get(b.u.id);
        if (!held && (!spot || this.sec - spot.t > LAST_SEEN)) return no(`hunt ${b.u.name}: no escort holds her, and she was last seen more than three minutes ago`);
        e.order = 'hunt';
        if (e.holds === b.u.id) { this.beginHunt(e, 'order'); return { ok: true, u, does: [`hunts ${b.u.name}`, `hunt ${b.u.name}`] }; }
        e.prey = b.u.id;
        const was = e.holds >= 0 ? this.boatOf.get(e.holds)!.u.name : '';
        if (e.holds >= 0) this.drop(e, 'left');
        e.chase = b.u.id;
        e.dx = held ? b.u.x : spot!.x; e.dy = held ? b.u.y : spot!.y;
        e.doing = 'run'; e.huntFrom = this.sec; e.seenAt = this.sec; e.attacks = 0;
        this.tally.hunts++;
        const where = held ? '' : ' where she was last seen';
        return { ok: true, u, does: was ? [`lets ${was} go and hunts ${b.u.name}${where}`, `let ${was} go and hunt ${b.u.name}${where}`] : [`hunts ${b.u.name}${where}`, `hunt ${b.u.name}${where}`] };
      }
      case 'convoy:screen': case 'hold': this.goBack(e); return { ok: true, u, does: ['goes back to its station round the convoy', 'go back to their stations round the convoy'] };
      case 'convoy:rescue': {
        const t = this.byId.get(c.target);
        if (!t || t.state !== 'gone') return no(`pick up survivors there: ${t ? `${t.name} is still afloat` : 'there is no wreck there'}`);
        this.goBack(e); e.doing = 'rescue'; e.order = 'rescue'; e.tx = t.x; e.ty = t.y; e.until = this.sec + 600 + Math.round(dist(e.u.x, e.u.y, t.x, t.y) / E.top);
        this.later.push({ at: e.until - 300, run: () => { if (e.doing === 'rescue') this.say('sea:rescue', e.u, `${e.u.name} picks up survivors.`, { on: t.id, by: e.u.id, wreck: { unit: t.id, ship: 1 } }, undefined, 'def', Math.round(e.u.x), Math.round(e.u.y)); } });
        return { ok: true, u, does: [`goes to pick up survivors from ${t.name}`, `go to pick up survivors from ${t.name}`] };
      }
      case 'convoy:starshell': {
        if (this.sec < e.nextShell) { const m = Math.ceil((e.nextShell - this.sec) / 60); return no(`fire star shell yet: the next is ready in ${m === 1 ? 'a minute' : `${m} minutes`}`); }
        e.nextShell = this.sec + 180;
        this.say('sea:starshell', e.u, `${e.u.name} fires star shell.`, { by: e.u.id, x: Math.round(c.x), y: Math.round(c.y) }, 'starshell', 'def', Math.round(c.x), Math.round(c.y));
        this.zones.push({ id: ++this.zoneId, kind: 'starshell', x: c.x, y: c.y, r: 800, until: this.sec + 60, side: 'def', unit: e.u.id });
        return { ok: true, u };
      }
      case 'move': {
        if (e.holds >= 0) this.drop(e, 'left');
        e.doing = 'move'; e.order = 'move'; e.tx = c.x; e.ty = c.y; e.chase = -1; e.prey = -1;
        return { ok: true, u, does: ['steams to the spot to search there', 'steam to the spot to search there'] };
      }
      default: return { ok: false, u };
    }
  }

  private orderBoat(b: Boat, c: Command | ConvoyCommand): Reply {
    const u = b.u, no = (why: string): Reply => ({ ok: false, u, says: `${u.name} can't ${why}.` });
    // (forced up: too badly damaged to dive only when she is doomed; else she may dive again once repaired, and says so)
    const up = b.doing === 'up' || b.doing === 'gun';
    // (an order's change of depth is always said, though she was on a duck left unsaid: it is the order's answer)
    b.duckQuiet = false;
    const hurt = (what: string): Reply => no(b.doomed ? `${what}: she was forced to the surface and is too badly damaged to dive` : `${what} while she is being repaired`);
    switch (c.do) {
      case 'convoy:attack': case 'attack': {
        const target = 'target' in c && c.target !== undefined ? this.byId.get(c.target) : undefined;
        if (target && target.side !== 'def') return no(`attack ${target.name}: she is one of the pack`);
        if (target && !live(target)) return no(`attack ${target.name}: she has already gone down`);
        if (up) return hurt('attack');
        if (b.tubes === 0 && b.load === 0) return no('attack: she has no torpedoes left');
        // The depth: as ordered; else the one her commander chose last; else the one she is at.
        const asked = 'under' in c && typeof c.under === 'boolean' ? c.under : undefined;
        const under = asked ?? (b.depth !== null ? b.depth === 'under' : b.dive !== 'surfaced');
        b.depth = under ? 'under' : 'surface';
        b.order = under ? 'under' : 'attack'; b.target = target?.id ?? -1;
        if (under) { if (b.dive === 'deep') this.setDive(b, 'periscope', 'attack'); } else this.setDive(b, 'surfaced', 'attack');
        b.doing = 'close';
        const at = `${target ? ` ${target.name}` : ''}${under ? ' submerged' : ' on the surface'}`;
        return { ok: true, u, does: b.tubes > 0 ? [`attacks${at}`, `attack${at}`] : [`closes to attack${at} once her tubes are reloaded`, `close to attack${at} once their tubes are reloaded`] };
      }
      case 'convoy:dive': {
        if (up) return hurt('dive');
        b.order = c.deep ? 'deep' : 'dive'; b.depth = 'under';
        const moved = this.setDive(b, c.deep ? 'deep' : 'periscope', 'ordered');
        b.doing = c.deep ? 'deep' : 'evade'; b.until = this.sec + 300;
        return moved ? { ok: true, u } : { ok: true, u, says: `${u.name} is already ${c.deep ? 'deep' : 'at periscope depth'}.` };
      }
      case 'convoy:surface': {
        if (up) return { ok: false, u, says: this.atSurface(b) ? `${u.name} is on the surface already: she was forced up.` : `${u.name} is coming up to the surface already: she was forced up.` };
        b.order = 'surface'; b.depth = 'surface';
        const moved = this.setDive(b, 'surfaced', 'ordered');
        b.doing = b.tubes > 0 ? 'close' : 'away';
        return moved ? { ok: true, u } : { ok: true, u, says: `${u.name} stays on the surface.` };
      }
      case 'convoy:evade': {
        if (up) return hurt('evade');
        b.order = 'evade';
        if (b.dive === 'surfaced') this.setDive(b, 'periscope', 'ordered');
        b.doing = 'evade'; b.until = this.sec + 600;
        return { ok: true, u, does: ['turns away to evade', 'turn away to evade'] };
      }
      case 'convoy:gun': {
        const t = this.byId.get(c.target);
        if (!t || t.side !== 'def') return no('fire her deck gun at that');
        if (!live(t)) return no(`fire her deck gun at ${t.name}: she has already gone down`);
        if (b.abandoned) return no('fire her deck gun: her crew have left her');
        if (b.dive !== 'surfaced') return no('fire her deck gun while she is dived: surface her first');
        const d = dist(u.x, u.y, t.x, t.y);
        if (d > S.gunAt) return no(`fire her deck gun at ${t.name}: the ship is ${metres(Math.round(d / 50) * 50)} away, and the gun reaches ${metres(S.gunAt)}`);
        b.order = 'gun'; b.target = t.id;
        return { ok: true, u, does: [`turns her deck gun on ${t.name}`, `turn their deck guns on ${t.name}`] };
      }
      case 'convoy:leave': case 'withdraw': {
        if (up) return hurt('leave the night');
        this.leave(b, 'ordered');
        return { ok: true, u };
      }
      default: return { ok: false, u };
    }
  }

  /**
   * Hand it to the staff (ruling 217(4), b348): the rest of the night is settled at once, as the computer's own nights
   * for the card settle their rest from this hour on (calibrate.ts), rolled on the battle's stream. The night's measures
   * take the staff's share too, so the after card's lines count the same night as its counts.
   */
  private handToStaff(): void {
    this.handed = true;
    this.handedAt = this.sec;
    const rest = this.rest?.(this.sec);
    if (rest) {
      const got: ConvoyCounts = { ss: {}, dd: {}, mv: 0 };
      for (const [k, v] of Object.entries(rest.ss)) { const n = this.rng.roll(v); if (n) { this.counts.ss[+k] = (this.counts.ss[+k] ?? 0) + n; got.ss[+k] = n; } }
      for (const [k, v] of Object.entries(rest.dd)) { const n = this.rng.roll(v); if (n) { this.counts.dd[+k] = (this.counts.dd[+k] ?? 0) + n; got.dd[+k] = n; } }
      got.mv = this.rng.roll(rest.mv);
      this.counts.mv += got.mv;
      this.staffPart = { counts: got, tally: this.splitRest(got, rest.tally ?? {}) };
    }
    this.finish('withdrew');
  }

  /**
   * The staff's dice for the rest of the night split into the measures the after card reads, so that they add up with
   * its counts: each boat lost given a way she went at the staff's own nights' shares for the rest of the night, each
   * ship sunk a torpedo that hit, and salvoes, hunts, attacks and boats forced up at their own means, never fewer than
   * those losses need. Rolled on the battle's stream after the counts, so it moves nothing booked.
   */
  private splitRest(got: ConvoyCounts, mean: Partial<ConvoyTally>): ConvoyTally {
    const add: ConvoyTally = { attacks: 0, hunts: 0, salvoes: 0, hits: 0, forcedUp: 0, kept: 0, escaped: 0, scuttled: 0, rammed: 0, shelled: 0, crushed: 0, captured: 0 };
    const ways = ['crushed', 'scuttled', 'rammed', 'shelled', 'captured'] as const;
    const weight = ways.map((k) => Math.max(0, mean[k] ?? 0)), all = weight.reduce((s, w) => s + w, 0);
    const boats = Object.values(got.ss).reduce((s, n) => s + n, 0), escorts = Object.values(got.dd).reduce((s, n) => s + n, 0);
    for (let i = 0; i < boats; i++) {
      let k = 0;
      if (all > 0) { let r = this.rng.next() * all; while (k < ways.length - 1 && r >= weight[k]) { r -= weight[k]; k++; } }
      add[ways[k]]++;
    }
    add.hits = got.mv + escorts;
    add.salvoes = Math.max(this.rng.roll(mean.salvoes ?? 0), Math.ceil(add.hits / B.salvo));
    add.escaped = this.rng.roll(mean.escaped ?? 0);
    add.forcedUp = Math.max(this.rng.roll(mean.forcedUp ?? 0), add.scuttled + add.rammed + add.shelled + add.captured + add.escaped);
    add.attacks = Math.max(this.rng.roll(mean.attacks ?? 0), add.crushed + add.scuttled + add.captured);
    add.hunts = Math.max(this.rng.roll(mean.hunts ?? 0), add.attacks > 0 ? 1 : 0);
    for (const k of Object.keys(add) as (keyof ConvoyTally)[]) this.tally[k] += add[k];
    return add;
  }

  /**
   * Dawn, or no boat left in it: the night ends. A hunt still alive at dawn ends in one last attack's roll; a boat it
   * sinks is told on the after card, since the night's screen gives way to it at that second.
   */
  private finish(how: 'time' | 'withdrew'): void {
    if (this.over) return;
    const dawn: string[] = [];
    if (how === 'time' && !this.handed) {
      for (const b of this.boats) {
        if (b.doing === 'lost' || b.doing === 'left' || b.doing === 'out' || b.doing === 'up' || b.doing === 'gun') continue;
        const hunters = this.huntersOf(b).filter((e) => e.echo?.kind === 'asdic');
        if (!hunters.length) continue;
        const e = hunters[0];
        if (this.rng.chance(attackKills({ ...e.kit, hedgehog: false }, hunters.length >= 2 || e.u.extra.ships > 1))) {
          this.boatLost(b, 'crushed', e.u.id);
          dawn.push(`At dawn ${e.u.name}'s last attack destroyed ${b.u.name}.`);
        }
      }
    }
    const lostB = Object.values(this.counts.ss).reduce((s, n) => s + n, 0), lostE = Object.values(this.counts.dd).reduce((s, n) => s + n, 0);
    // A plain verdict for the screens, the level's own (levelOfNight); what the campaign books is the night's rank (calibrate.ts).
    const level = levelOfNight(this.counts.mv, this.merchants.length, lostB, this.boats.length, lostE);
    const winner: Side = /victory/.test(level) ? 'att' : 'def';
    const extra: ConvoyOverExtra = { counts: this.counts, tally: this.tally, handed: this.handed, steered: { ...this.steered }, ...(this.handed ? { handedAt: this.handedAt, staff: this.staffPart } : {}) };
    this.over = {
      // (a night handed over runs to dawn: the staff fought the rest of it)
      winner, how, seconds: this.handed ? Math.max(this.sec, this.limit) : this.sec, R: winner === 'att' ? 1.2 : 0.9, // (words only: the night books by its ranks, calibrate.ts)
      lost: { att: lostB / Math.max(1, this.boats.length), def: (this.counts.mv + lostE) / Math.max(1, this.merchants.length + this.escorts.length) },
      men: { att: lostB * CONVOY.crew.ss, def: this.counts.mv * CONVOY.crew.mv },
      level,
      lines: this.closing(lostB, lostE, dawn),
      forces: this.ticket.place.fleets.map((f) => ({ corps: f.id, side: f.side, lost: f.side === 'att' ? this.counts.ss[f.id] ?? 0 : this.counts.dd[f.id] ?? 0 })),
      extra: extra as unknown as Record<string, unknown>,
    };
    this.events.push({ t: this.sec, what: 'end', text: how === 'time' ? 'Dawn. The pack draws off.' : 'The night is over.' });
  }

  /** The after card's words: what the night cost each side, and how the boats lost went (words only). */
  private closing(lostB: number, lostE: number, dawn: string[] = []): string[] {
    const sunk = this.counts.mv, ships = this.merchants.length;
    const lines = [sunk === 0 ? 'The convoy comes through the night without loss.' : `${sunk} ${sunk === 1 ? 'merchant' : 'merchants'} of ${ships} sunk.`];
    lines.push(...fateWords(this.tally, lostB, this.boats.length), ...dawn);
    if (lostE) lines.push(`${lostE} ${lostE === 1 ? 'escort' : 'escorts'} lost.`);
    if (this.handed) lines.push(`The staff fought the night from ${afterDusk(this.handedAt)} to dawn; these lines count the whole night.`);
    return lines;
  }

  // ---- The picture --------------------------------------------------------------------------------------------------

  /** Every boat the night's card brings, those still to come and those gone included. */
  get packSize(): number { return this.boats.length; }

  /**
   * The picture as one side sees it (or all of it): its own pieces, the convoy, the pieces of the other side it knows
   * of now, and those it lost touch with in the last LAST_SEEN at their last known spots. A side hears what its own
   * pieces did and what was done to them: the pack hears the depth charges on its own boats, told from the boat. The
   * page's own notes (orders answered, a boat found, the first salvo) go only to a side's picture: the scene reads none.
   */
  view(as?: Side): ConvoyView {
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
    let told: FieldEvent[];
    if (as === undefined) told = events.filter((e) => !PAGE_ONLY.has(e.what));
    else {
      told = [];
      for (const e of events) {
        const on = e.what === 'sea:depthCharge' ? this.boatOf.get((e.extra as { on?: number } | undefined)?.on ?? -1) : undefined;
        if (on && on.u.side === as && e.side !== as) {
          told.push({ ...e, unit: on.u.id, side: as, text: (e.extra as { kind?: string }).kind === 'ahead' ? `${on.u.name} is attacked with ahead-throwers.` : `${on.u.name} is depth-charged.` });
          continue;
        }
        if (e.side === as || e.what === 'contact' || e.what === 'end' || e.what === 'sea:attack' || e.what === 'sea:torpedoHit' || e.what === 'sea:sunk' || (e.unit !== undefined && this.byId.get(e.unit)?.side === as)) told.push(e);
      }
    }
    const lastSeen = as === undefined ? [] : [...this.spots[as].values()].filter((p) => !this.known[as].has(p.id) && this.sec - p.t <= LAST_SEEN && this.byId.get(p.id)!.state !== 'gone').map((p) => ({ ...p }));
    return {
      t: this.t, limit: this.limit, units, balance: { att: 1 - afloat, def: afloat },
      events: told,
      zones: as === undefined ? this.zones.slice() : this.zones.filter((z) => z.side === as || z.kind === 'oil' || z.kind === 'starshell'),
      ai: { ...this.ai }, over: this.over, contact: this.contactAt, lastSeen, pack: this.boats.length,
    };
  }
}
