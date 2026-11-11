export const G = 9.80665;
export const DEG = Math.PI / 180;
export const TAU = Math.PI * 2;

export const clamp = (v: number, lo: number, hi: number): number => (v < lo ? lo : v > hi ? hi : v);
export const lerp = (a: number, b: number, t: number): number => a + (b - a) * t;

/** Wraps an angle into (-PI, PI]. */
export function wrapAngle(a: number): number {
  a = a % TAU;
  if (a > Math.PI) a -= TAU;
  else if (a <= -Math.PI) a += TAU;
  return a;
}

export const dist2 = (ax: number, ay: number, bx: number, by: number): number => Math.hypot(ax - bx, ay - by);
export const dist3 = (ax: number, ay: number, az: number, bx: number, by: number, bz: number): number =>
  Math.hypot(ax - bx, ay - by, az - bz);

/** ISA air density in kg/m3 at altitude h metres. */
export function airDensity(h: number): number {
  if (h < 0) h = 0;
  if (h > 11000) return 0.36392 * Math.exp(-(h - 11000) / 6341.6);
  return 1.225 * Math.pow(1 - 2.25577e-5 * h, 4.25588);
}

export const M_TO_FT = 3.28084;
export const MS_TO_MPH = 2.23694;
export const MS_TO_KMH = 3.6;

/** Compass bearing in degrees (0 = north, 90 = east) from a maths angle (0 = east, counter-clockwise). */
export function compassDeg(theta: number): number {
  let d = 90 - theta / DEG;
  d %= 360;
  if (d < 0) d += 360;
  return d;
}
