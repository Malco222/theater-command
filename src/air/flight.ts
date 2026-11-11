// The flight model. Every aircraft is a point mass flying on its real numbers:
// weight, wing area, engine power by height, and drag worked out from its documented
// top speed. Climb, turn and dive performance are not typed in anywhere. They come out
// of the physics, and they change with altitude the way the real aircraft did.

import { G, airDensity, clamp, wrapAngle } from '../core/math';
import rawAircraft from '../../data/aircraft.json';

export interface GunDef {
  name: string;
  count: number;
  /** Rounds per second, per gun. */
  rof: number;
  dmg: number;
  ammoSec: number;
}

export interface AircraftType {
  key: string;
  name: string;
  short: string;
  role: 'fighter' | 'heavy_fighter' | 'bomber';
  nation: string;
  mass: number;
  W: number;
  S: number;
  span: number;
  length: number;
  engines: number;
  powerW: number;
  critAlt: number;
  slFrac: number;
  eta: number;
  cd0: number;
  K: number;
  clMax: number;
  gLimit: number;
  rollRate: number;
  vne: number;
  ceiling: number;
  cruise: number;
  climbIas: number;
  floatCarb: boolean;
  fuelKg: number;
  burnCruise: number;
  burnCombat: number;
  guns: GunDef[];
  defGuns?: { nose: number; dorsal: number; ventral: number; beam: number };
  durability: number;
  pilotArmor: number;
  selfSealing: boolean;
  crew: number;
  bombKg: number;
  refClimbSl: number;
  /** Total hitting power per second of fire with every gun firing. */
  firepower: number;
}

/** Engine power in watts at height h, full throttle. */
export function enginePower(t: AircraftType, h: number): number {
  if (h <= t.critAlt) return t.powerW * (t.slFrac + (1 - t.slFrac) * (h / t.critAlt));
  return t.powerW * (airDensity(h) / airDensity(t.critAlt));
}

function buildType(key: string, d: any): AircraftType {
  const W = d.massKg * G;
  const AR = (d.spanM * d.spanM) / d.wingAreaM2;
  const K = 1 / (Math.PI * d.oswald * AR);
  const t: AircraftType = {
    key, name: d.name, short: d.short, role: d.role, nation: d.nation,
    mass: d.massKg, W, S: d.wingAreaM2, span: d.spanM, length: d.lengthM,
    engines: d.engines, powerW: d.powerKw * 1000, critAlt: d.critAltM, slFrac: d.slPowerFrac ?? 0.88,
    eta: d.propEff ?? 0.8, cd0: 0, K, clMax: d.clMax, gLimit: d.gLimit,
    rollRate: (d.rollRateDeg * Math.PI) / 180, vne: d.vneMs, ceiling: d.ceilingM,
    cruise: d.cruiseMs, climbIas: d.climbIasMs, floatCarb: !!d.floatCarb,
    fuelKg: d.fuelKg, burnCruise: d.burnCruiseKgS, burnCombat: d.burnCombatKgS,
    guns: d.guns ?? [], defGuns: d.defGuns, durability: d.durability, pilotArmor: d.pilotArmor,
    selfSealing: !!d.selfSealing, crew: d.crew ?? 1, bombKg: d.bombKg ?? 0, refClimbSl: d.refClimbSlMs ?? 0,
    firepower: 0,
  };
  // Solve the zero-lift drag coefficient from the documented top speed at height:
  // in level flight at top speed, thrust power equals drag power.
  const rho = airDensity(d.vmaxAltM);
  const qS = 0.5 * rho * d.vmaxMs * d.vmaxMs * t.S;
  const thrust = (t.eta * enginePower(t, d.vmaxAltM)) / d.vmaxMs;
  const induced = (K * W * W) / qS;
  t.cd0 = Math.max(0.012, (thrust - induced) / qS);
  t.firepower = t.guns.reduce((s, g) => s + g.count * g.rof * g.dmg, 0);
  return t;
}

export const AIRCRAFT: Record<string, AircraftType> = {};
for (const [key, d] of Object.entries(rawAircraft as Record<string, any>)) {
  if (key.startsWith('_')) continue;
  AIRCRAFT[key] = buildType(key, d);
}

/** Stall speed (true airspeed) at height h. */
export function stallSpeed(t: AircraftType, h: number): number {
  return Math.sqrt((2 * t.W) / (airDensity(h) * t.S * t.clMax));
}

/** Specific excess power in m/s: how fast the aircraft can gain height at this speed, height and load factor. */
export function excessPower(t: AircraftType, v: number, h: number, n: number, thr: number, powerMul = 1): number {
  const rho = airDensity(h);
  const qS = 0.5 * rho * v * v * t.S;
  const thrust = (t.eta * enginePower(t, h) * thr * powerMul) / Math.max(v, 40);
  const drag = qS * t.cd0 + (t.K * n * n * t.W * t.W) / qS;
  return ((thrust - drag) * v) / t.W;
}

/** Best steady climb rate in m/s at height h (searches over speed). */
export function climbRate(t: AircraftType, h: number): number {
  let best = -99;
  const vs = stallSpeed(t, h);
  for (let v = vs * 1.15; v < t.vne; v += 2) best = Math.max(best, excessPower(t, v, h, 1, 1));
  return best;
}

/** Top level speed in m/s at height h. */
export function levelSpeed(t: AircraftType, h: number): number {
  let lo = stallSpeed(t, h), hi = 260;
  for (let i = 0; i < 40; i++) {
    const mid = (lo + hi) / 2;
    if (excessPower(t, mid, h, 1, 1) > 0) lo = mid; else hi = mid;
  }
  return lo;
}

/** Best sustained turn rate in degrees per second at height h. */
export function sustainedTurn(t: AircraftType, h: number): { rateDeg: number; radius: number; speed: number } {
  let best = { rateDeg: 0, radius: 0, speed: 0 };
  const rho = airDensity(h);
  for (let v = stallSpeed(t, h) * 1.05; v < 180; v += 1) {
    const nStall = (0.5 * rho * v * v * t.S * t.clMax) / t.W;
    let lo = 1, hi = Math.min(nStall, t.gLimit);
    if (hi <= 1 || excessPower(t, v, h, 1, 1) < 0) continue;
    if (excessPower(t, v, h, hi, 1) < 0) {
      for (let i = 0; i < 30; i++) {
        const mid = (lo + hi) / 2;
        if (excessPower(t, v, h, mid, 1) > 0) lo = mid; else hi = mid;
      }
    } else lo = hi;
    const w = (G * Math.sqrt(lo * lo - 1)) / v;
    const rateDeg = (w * 180) / Math.PI;
    if (rateDeg > best.rateDeg) best = { rateDeg, radius: v / w, speed: v };
  }
  return best;
}

/** The moving state the flight model needs. Aircraft objects in the sim carry these fields. */
export interface FlightState {
  type: AircraftType;
  x: number; y: number; z: number;
  v: number;
  hdg: number;
  gam: number;
  bank: number;
  thr: number;
  /** What the pilot is asking for this tick. */
  cmdHdg: number;
  cmdGam: number;
  cmdG: number;
  /** 0..1 engine health, and a drag multiplier from damage. */
  powerMul: number;
  dragMul: number;
  /** Seconds of engine cut left after a negative-G push in a float-carburettor engine. */
  cutT: number;
  /** Load factor actually pulled this tick (for fuel, fatigue and gunnery). */
  n: number;
}

const K_TURN = 2.2;
const K_PITCH = 1.6;

/** Advances one aircraft by dt seconds. */
export function stepFlight(a: FlightState, dt: number): void {
  const t = a.type;
  const rho = airDensity(a.z);
  const v = a.v;
  const qS = 0.5 * rho * v * v * t.S;
  const nStall = (qS * t.clMax) / t.W;
  const cap = Math.max(0.2, Math.min(a.cmdG, t.gLimit, nStall));
  const cg = Math.cos(a.gam);

  // What the pilot wants, as rates.
  const dPsi = wrapAngle(a.cmdHdg - a.hdg);
  const wWant = dPsi * K_TURN;
  const gWant = clamp(a.cmdGam - a.gam, -1.2, 1.2) * K_PITCH;

  // As load factor: sideways (turn) and in the vertical plane (pull or push).
  let nh = (v * wWant * Math.max(cg, 0.2)) / G;
  let nv = cg + (v * gWant) / G;

  let n: number;
  let nhA: number;
  let nvA: number;
  const bunt = nv < 0.15 && Math.abs(nh) < 1.2;
  if (bunt && !t.floatCarb) {
    // Fuel-injected engines can simply push over into a dive.
    nvA = Math.max(nv, -1.5);
    nhA = clamp(nh, -1, 1);
    n = Math.abs(nvA);
    a.bank += clamp(0 - a.bank, -t.rollRate * dt, t.rollRate * dt);
  } else {
    n = Math.hypot(nh, nv);
    if (n > cap) { const s = cap / n; nh *= s; nv *= s; n = cap; }
    // Roll toward the bank that points the lift where it is wanted. A float-carburettor
    // engine cuts under negative G, so those pilots roll inverted and pull instead.
    const bankT = Math.atan2(nh, nv);
    let dB = wrapAngle(bankT - a.bank);
    const speedStiff = v > 130 ? Math.max(0.45, 1 - (v - 130) / 160) : 1;
    const rr = t.rollRate * speedStiff * dt;
    dB = clamp(dB, -rr, rr);
    a.bank = wrapAngle(a.bank + dB);
    // Only pull once the wings are close to where they need to be.
    const align = Math.max(0, Math.cos(wrapAngle(bankT - a.bank)));
    const nEff = 1 + (n - 1) * align;
    nhA = nEff * Math.sin(a.bank);
    nvA = nEff * Math.cos(a.bank);
    n = nEff;
  }

  if (a.cutT > 0) a.cutT -= dt;

  const w = (G * nhA) / (v * Math.max(cg, 0.2));
  const gamRate = (G * (nvA - cg)) / v;
  a.hdg = wrapAngle(a.hdg + w * dt);
  a.gam = clamp(a.gam + gamRate * dt, -1.45, 1.45);

  const power = a.cutT > 0 ? 0 : enginePower(t, a.z) * a.thr * a.powerMul;
  const thrust = (t.eta * power) / Math.max(v, 40);
  let drag = qS * t.cd0 * a.dragMul + (t.K * n * n * t.W * t.W) / qS;
  if (v > t.vne) drag *= 1 + (v - t.vne) * 0.08; // compressibility wall near the dive limit
  const vdot = (thrust - drag) / t.mass - G * Math.sin(a.gam);
  a.v = Math.max(25, v + vdot * dt);
  a.n = n;

  const sg = Math.sin(a.gam);
  const c2 = Math.cos(a.gam);
  a.x += a.v * c2 * Math.cos(a.hdg) * dt;
  a.y += a.v * c2 * Math.sin(a.hdg) * dt;
  a.z += a.v * sg * dt;
}

/**
 * Autopilot for formation leaders and aircraft flying somewhere without fighting:
 * hold a heading, climb or descend to a height, and hold a speed.
 */
export function navControl(a: FlightState, hdg: number, alt: number, speed: number, gCap: number): void {
  const t = a.type;
  a.cmdHdg = hdg;
  a.cmdG = gCap;
  const dz = alt - a.z;
  const rho = airDensity(a.z);
  if (dz > 120) {
    a.thr = 1;
    const ps = excessPower(t, a.v, a.z, 1, 1, a.powerMul);
    const vClimb = t.climbIas / Math.sqrt(rho / 1.225);
    let g = Math.asin(clamp(ps / a.v, -0.3, 0.45));
    g += clamp((a.v - vClimb) / vClimb, -0.4, 0.4) * 0.6;
    a.cmdGam = clamp(Math.min(g, dz / (a.v * 4)), -0.05, 0.42);
  } else if (dz < -120) {
    a.cmdGam = clamp(dz / (a.v * 12), -0.22, 0);
    a.thr = clamp(0.55 + 0.03 * (speed - a.v), 0.25, 1);
  } else {
    a.cmdGam = clamp(dz / (a.v * 6), -0.08, 0.08);
    a.thr = clamp(0.62 + 0.035 * (speed - a.v), 0.3, 1);
  }
}
