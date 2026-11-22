// The battle clock: pause, slow, fast, as the genre has it. The sims step in fixed ticks of DT, so a battle plays
// the same at any speed and on any machine; the clock only decides how many ticks a frame of real time is worth.

import { DT, type BattleSim } from './types';

export const SPEEDS = [0, 0.5, 1, 2, 4] as const;
export type Speed = (typeof SPEEDS)[number];

export class Clock {
  speed: Speed = 1;
  /** Real seconds owed to the sim, carried between frames. */
  private owed = 0;
  /** The most ticks one frame may run, so a long stall (a hidden tab) never runs the battle away. */
  maxTicks = 40;
  /**
   * Time runs this much faster on its own while nothing is happening yet (ruling 226(1): fast until the armies meet,
   * then the speed the player set). 1 is the speed shown; the screens set it and say so.
   */
  rush = 1;

  get paused(): boolean { return this.speed === 0; }

  /** Space: pause, or back to the speed before. */
  private before: Speed = 1;
  toggle(): void {
    if (this.speed === 0) this.speed = this.before;
    else { this.before = this.speed; this.speed = 0; }
  }

  set(s: Speed): void { if (s !== 0) this.before = s; this.speed = s; }

  /** Faster or slower by one step, never into pause. */
  nudge(dir: 1 | -1): void {
    const i = SPEEDS.indexOf(this.speed === 0 ? this.before : this.speed);
    this.set(SPEEDS[Math.max(1, Math.min(SPEEDS.length - 1, i + dir))]);
  }

  /** Runs the sim for a frame of real time. Returns the ticks run. */
  advance(sim: BattleSim, realSeconds: number): number {
    if (sim.over) return 0;
    this.owed += Math.max(0, realSeconds) * this.speed * (this.speed > 0 ? this.rush : 1);
    let n = 0;
    while (this.owed >= DT - 1e-9 && n < this.maxTicks && !sim.over) { sim.step(); this.owed -= DT; n++; }
    if (n >= this.maxTicks) this.owed = 0;
    return n;
  }
}

/** Runs a sim to its end with no screen at all (tests and the calibration tool). */
export function runOut(sim: BattleSim, most = 1e6): void {
  for (let i = 0; i < most && !sim.over; i++) sim.step();
}
