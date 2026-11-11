// Builds a battle from a scenario (which is what a Battle Ticket from the campaign
// map turns into) and holds the small helpers every other air module uses.
import { AIRCRAFT } from './flight';
import { ll, landClass } from './geo';
import { Rng } from '../core/rng';
import { makeName } from '../core/names';
import { DEG, clamp } from '../core/math';
import type { Aircraft, Contact, LogLine, Order, Pilot, ScenarioDef, SideId, Squadron, World } from './types';

export const DT = 0.1;

function slots(kind: 'vic' | 'schwarm' | 'kette', n: number): { x: number; y: number; z: number }[] {
  const out: { x: number; y: number; z: number }[] = [];
  if (kind === 'vic') {
    for (let s = 0; out.length < n; s++) {
      out.push({ x: -s * 95, y: 0, z: -s * 14 }, { x: -s * 95 - 28, y: 34, z: -s * 14 }, { x: -s * 95 - 28, y: -34, z: -s * 14 });
    }
  } else if (kind === 'schwarm') {
    const four = [[0, 0], [-50, 165], [-40, -185], [-90, -350]];
    for (let j = 0; out.length < n; j++) {
      const ox = -j * 240, oy = j === 0 ? 0 : (j % 2 ? 1 : -1) * 380 * Math.ceil(j / 2), oz = j * 130;
      for (const [fx, fy] of four) out.push({ x: ox + fx, y: oy + fy, z: oz });
    }
  } else {
    for (let k = 0; out.length < n; k++) {
      const ox = k === 0 ? 0 : -115 * Math.ceil(k / 2), oy = k === 0 ? 0 : (k % 2 ? 1 : -1) * 125 * Math.ceil(k / 2), oz = k === 0 ? 0 : -28;
      out.push({ x: ox, y: oy, z: oz }, { x: ox - 42, y: oy + 52, z: oz - 8 }, { x: ox - 42, y: oy - 52, z: oz - 8 });
    }
  }
  return out.slice(0, n);
}

function parseClock(s: string): number {
  const [h, m] = s.split(':').map(Number);
  return h * 3600 + m * 60;
}

export function createWorld(def: ScenarioDef, seed: number, human: SideId | -1, assist: boolean): World {
  const rng = new Rng(seed);
  const places = def.places.map((p) => ({ name: p.name, kind: p.kind, side: p.side, ...ll(p.lat, p.lon) }));
  const tgt = ll(def.target.lat, def.target.lon);
  places.push({ name: def.target.name, kind: 'target', side: undefined, x: tgt.x, y: tgt.y });
  const az = def.sun.azDeg * DEG, el = def.sun.elDeg * DEG;
  const w: World = {
    def, seed, rng, time: 0, tick: 0, clock0: parseClock(def.clock),
    sides: def.sides, squadrons: [], aircraft: [], contacts: [[], []], places,
    radars: def.radars.map((r) => ({ name: r.name, ...ll(r.lat, r.lon), range: r.rangeKm * 1000 })),
    flak: def.flak.map((f) => ({ name: f.name, ...ll(f.lat, f.lon), r: f.radiusKm * 1000, power: f.power })),
    target: { name: def.target.name, x: tgt.x, y: tgt.y },
    sun: { x: Math.sin(az) * Math.cos(el), y: Math.cos(az) * Math.cos(el), z: Math.sin(el) },
    events: [], log: [], human, assist,
    bombsDropped: 0, bombsHit: 0, bombsCarried: 0, shockDone: false, over: false, result: null,
    overHome: landClass,
  };
  const route = def.route.map(([lat, lon]) => ll(lat, lon));
  const escortSlots = new Map<string, number>();

  for (const sd of def.squadrons) {
    const type = AIRCRAFT[sd.type];
    const side = def.sides[sd.side];
    const basePlace = places.find((p) => p.name === sd.base) ?? places[0];
    const isBomber = type.role === 'bomber';
    const sq: Squadron = {
      id: w.squadrons.length, side: sd.side, name: sd.name, group: sd.group, raid: sd.raid ?? '', type,
      ac: [], leader: null, base: { name: basePlace.name, x: basePlace.x, y: basePlace.y },
      order: { k: 'hold' }, queue: [], alt: type.key === 'spitfire_mk1' ? 7300 : 5600,
      stance: sd.stance ?? 'fighters', auto: true, state: sd.air ? 'transit' : 'ready', stateT: 0,
      morale: 78 + sd.quality * 20, engaged: [], tgtSq: null, x: basePlace.x, y: basePlace.y, z: 0, hdg: 0, spd: 0,
      alive: sd.count, started: sd.count, lost: 0, kills: 0, quality: sd.quality,
      route: null, routeIdx: 0, bombed: false, bombsHit: 0, slot: 0,
      nav: { x: basePlace.x, y: basePlace.y, alt: 3000, spd: type.cruise, g: isBomber ? 1.25 : 2.2 },
      bingo: false, surprisedT: 0, shocked: false, thinkAt: rng.next() * 0.5,
      scrambleSec: (sd.scramble ?? 0) * (sd.side === 0 ? def.mods.scrambleMul : 1), isBomber, combatSec: 9999, sawEnemy: false,
    };
    const form = isBomber ? 'kette' : side.formation;
    const sl = slots(form, sd.count);
    const start = sd.air ? ll(sd.air.lat, sd.air.lon) : { x: basePlace.x, y: basePlace.y };
    const hdg = sd.air ? (90 - sd.air.hdgDeg) * DEG : 0;
    for (let i = 0; i < sd.count; i++) {
      const nm = makeName(rng, sd.pool ?? side.namePool, i === 0);
      const skill = clamp(sd.quality + rng.normal() * 0.11 + (i === 0 ? 0.1 : 0), 0.15, 0.97);
      const pilot: Pilot = {
        name: nm.name, rank: nm.rank, skill,
        kills: skill > 0.78 ? 2 + rng.int(7) : skill > 0.62 ? rng.int(4) : 0,
        killsNow: 0, fatigue: sd.side === 0 ? def.mods.defenderFatigue : 0, fate: null,
      };
      const s = sl[i];
      const c = Math.cos(hdg), sn = Math.sin(hdg);
      const ac: Aircraft = {
        id: w.aircraft.length, sq, side: sd.side, pilot, type,
        x: start.x + c * s.x - sn * s.y, y: start.y + sn * s.x + c * s.y, z: sd.air ? sd.air.alt + s.z : 0,
        px: 0, py: 0, pz: 0, v: sd.air ? type.cruise : 0, hdg, gam: 0, bank: 0, thr: 0.65,
        cmdHdg: hdg, cmdGam: 0, cmdG: 2, powerMul: 1, dragMul: 1, cutT: 0, n: 1,
        mode: sd.air ? 'form' : 'ground', alive: true, falling: false, gone: false,
        hp: type.durability, fuel: type.fuelKg * (sd.fuelFrac ?? 1), ammo: type.guns.map((g) => g.ammoSec),
        onFire: false, leak: 0, enginesOut: 0, smoking: false, damaged: false,
        slotX: s.x, slotY: s.y, slotZ: s.z, tgt: null, threat: null, ai: 'attack', aiT: 0,
        thinkAt: rng.next() * 0.2, firing: false, aware: false, bombs: type.bombKg > 0, hunters: 0, spin: 0,
      };
      ac.px = ac.x; ac.py = ac.y; ac.pz = ac.z;
      sq.ac.push(ac);
      w.aircraft.push(ac);
    }
    sq.leader = sq.ac[0];
    if (sd.air) { sq.x = start.x; sq.y = start.y; sq.z = sd.air.alt; sq.hdg = hdg; sq.spd = type.cruise; sq.alt = sd.air.alt; }
    if (isBomber) { w.bombsCarried += sd.count; sq.route = route; sq.routeIdx = 1; sq.order = { k: 'bomb' }; sq.alt = sd.air?.alt ?? def.bombAlt; }
    w.squadrons.push(sq);
  }
  // Escort orders need the charge's id, so they are wired up after every unit exists.
  def.squadrons.forEach((sd, i) => {
    const sq = w.squadrons[i];
    if (!sd.order || sd.order === 'bomb') return;
    const charge = w.squadrons.find((s) => s.name === sd.charge);
    if (!charge) return;
    const n = escortSlots.get(charge.name + sd.order) ?? 0;
    escortSlots.set(charge.name + sd.order, n + 1);
    sq.slot = n + (sd.order === 'top' ? 10 : 0);
    sq.order = { k: 'escort', sq: charge.id, mode: sd.order === 'free' ? 'free' : 'close' } as Order;
    sq.stance = 'fighters';
  });
  for (const s of [0, 1] as const) {
    w.contacts[s] = w.squadrons.map((): Contact => ({ level: 0, src: '', x: 0, y: 0, z: 0, hdg: 0, spd: 0, est: 0, known: false, t: -999, nextPlot: 0 }));
  }
  return w;
}

export function say(w: World, side: SideId | -1, kind: LogLine['kind'], text: string, x?: number, y?: number): void {
  w.log.push({ time: w.time, side, kind, text, x, y });
  if (w.log.length > 400) w.log.splice(0, 100);
}

/** Aircraft that still count toward a unit's fighting strength. */
export function effective(sq: Squadron): Aircraft[] {
  return sq.ac.filter((a) => a.alive && a.mode !== 'out');
}

export function airborne(sq: Squadron): boolean {
  return sq.state === 'transit' || sq.state === 'engaged' || sq.state === 'reform' || sq.state === 'rtb';
}

export function clockText(w: World): string {
  const t = Math.floor(w.clock0 + w.time);
  const h = Math.floor(t / 3600) % 24, m = Math.floor(t / 60) % 60, s = t % 60;
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

export function nearestPlace(w: World, x: number, y: number): string {
  let best = '', bd = 1e12;
  for (const p of w.places) {
    if (p.kind === 'radar' || p.kind === 'field') continue;
    const d = (p.x - x) ** 2 + (p.y - y) ** 2;
    if (d < bd) { bd = d; best = p.name; }
  }
  const km = Math.sqrt(bd) / 1000;
  if (km < 6) return `over ${best}`;
  if (w.overHome(x, y) === 0) return km < 30 ? `off ${best}` : 'over the Channel';
  return `near ${best}`;
}

export function moraleLabel(m: number): 'Steady' | 'Wavering' | 'Broken' {
  return m >= 50 ? 'Steady' : m >= 25 ? 'Wavering' : 'Broken';
}
