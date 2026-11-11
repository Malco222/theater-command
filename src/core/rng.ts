// Deterministic random numbers. Every roll in the simulation comes from a seeded
// generator, never Math.random, so the same seed and the same orders always replay
// the same battle. That is what keeps auto-resolve fair and makes replays and
// multiplayer possible later.

export class Rng {
  private s: number;

  constructor(seed: number) {
    this.s = seed >>> 0 || 0x9e3779b9;
  }

  /** Uniform in [0, 1). mulberry32. */
  next(): number {
    let t = (this.s = (this.s + 0x6d2b79f5) >>> 0);
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }

  range(a: number, b: number): number {
    return a + (b - a) * this.next();
  }

  int(n: number): number {
    return Math.floor(this.next() * n);
  }

  chance(p: number): boolean {
    return this.next() < p;
  }

  pick<T>(arr: readonly T[]): T {
    return arr[this.int(arr.length)];
  }

  /** Roughly normal, mean 0, sd 1 (sum of uniforms; cheap and deterministic). */
  normal(): number {
    return (this.next() + this.next() + this.next() + this.next() - 2) * 1.7320508;
  }

  /** Turns an expected count into a whole number of events. */
  roll(expected: number): number {
    const whole = Math.floor(expected);
    return whole + (this.next() < expected - whole ? 1 : 0);
  }

  /** An independent stream, so adding rolls in one system does not shift another. */
  fork(label: number): Rng {
    return new Rng((Math.imul(this.s ^ 0x85ebca6b, 0xc2b2ae35) + label * 0x27d4eb2f) >>> 0);
  }

  get state(): number {
    return this.s;
  }
}

export function hashString(str: string): number {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}
