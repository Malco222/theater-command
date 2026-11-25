// Release 2's own water and the player's orders (the design director, 25 November 13:22Z, Q1 and Q2): a ship keeping
// clear of her own side's torpedoes is interrupted, never re-ordered, and takes up the player's order unchanged once
// they have passed; a destroyer piece the player sends at a ship holds its salvo while one of its own ships is in the
// way, keeps the order and its run, and fires the moment the water is clear. Positions are set by hand between steps,
// as the field's own tests do.
import { describe, expect, it } from 'vitest';
import { DT } from '../src/battle/core/types';
import { SeaSim } from '../src/battle/sea/sim';
import { plainTicket, type PlainFleet } from '../src/battle/sea/ticket';
import type { SeaUnit } from '../src/battle/sea/types';

const OURS: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 1, dd: 4 } };
const THEIRS: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 1 } };

function field(ownWater: boolean): { sim: SeaSim; s: any; bb: SeaUnit; dd: SeaUnit; foe: SeaUnit } {
  const sim = new SeaSim(plainTicket({ att: [OURS], def: [THEIRS], seed: 7, human: 'att', ownWater }));
  const s = sim as any;
  const bb = sim.units.find((u) => u.side === 'att' && u.extra.cls === 'bb')!;
  const dd = sim.units.find((u) => u.side === 'att' && u.extra.cls === 'dd')!;
  const foe = sim.units.find((u) => u.side === 'def' && u.extra.cls === 'bb')!;
  return { sim, s, bb, dd, foe };
}
const put = (u: SeaUnit, x: number, y: number, face: number, way: number): void => { u.x = x; u.y = y; u.face = face; u.extra.way = way; };
const steps = (sim: SeaSim, seconds: number, each?: () => void): void => {
  for (let i = 0; i < Math.round(seconds / DT) && !sim.over; i++) { sim.step(); each?.(); }
};
const turnTo = (a: number, b: number): number => { let d = b - a; while (d > Math.PI) d -= 2 * Math.PI; while (d < -Math.PI) d += 2 * Math.PI; return d; };

describe("own water and the player's orders", () => {
  /**
   * The battleship is sent west across the water ahead of a salvo her own destroyers have fired north. With the rule on,
   * she turns square off its tracks on her side (east) while its heads are short of her, her order the same object all
   * the while, and the look fields say why and that the order stands; once they are past she steers for the point she
   * was sent to again. With the rule off she holds her course west into the tracks.
   */
  for (const on of [true, false]) {
    it(`${on ? 'with the rule on' : 'with the rule off'}, a ship sent somewhere by the player ${on ? 'turns clear of her own side\'s torpedoes and then takes up the same order again' : 'keeps her course through her own side\'s torpedo water'}`, () => {
      const { sim, s, bb, dd, foe } = field(on);
      put(dd, 5000, 5000, 0, 0);
      put(foe, 5000, 11000, Math.PI / 2, 0);
      put(bb, 5900, 9000, -Math.PI / 2, 8);
      sim.command({ do: 'hold', units: [dd.id] }, 'att');
      expect(sim.command({ do: 'move', units: [bb.id], x: 2000, y: 9000 }, 'att')).toBe(true);
      const order = sim.orderOf(bb.id);
      s.launch(dd, foe, null);
      const salvo = s.torps.find((t: any) => t.from === dd.id);
      expect(salvo).toBeTruthy();
      expect(salvo.weight).toBeGreaterThan(0);
      let turned = false, ordered = false, from = -1, until = -1, leastWest = Infinity;
      steps(sim, 8, () => {
        if (bb.extra.clearFrom !== undefined) { turned = true; from = bb.extra.clearFrom as number; until = bb.extra.clearUntil as number; ordered = bb.extra.ordered === true; }
        leastWest = Math.min(leastWest, Math.abs(turnTo(bb.face, -Math.PI / 2)));
        expect(sim.orderOf(bb.id)).toBe(order);
      });
      if (on) {
        expect(turned).toBe(true);
        expect(from).toBe(dd.id);
        // 'clearUntil' is the second the heads pass her where she lies (when she is let go), never after the salvo lands.
        expect(until).toBeGreaterThan(s.t);
        expect(until).toBeLessThanOrEqual(salvo.at + 1e-9);
        expect(ordered).toBe(true);
        expect(typeof bb.extra.clearFromName).toBe('string');
        // Square off the tracks on her side: her heading has swung from west toward east (north-up, the tracks run north).
        expect(Math.abs(turnTo(bb.face, Math.PI / 2))).toBeLessThan(Math.abs(turnTo(-Math.PI / 2, Math.PI / 2)));
        expect((s.tally['att:clear'] ?? 0)).toBeGreaterThanOrEqual(1);
        // She is let go within a tick or two of the last 'clearUntil' she showed (the scene's countdown reaches 0 as she
        // takes up her order), and past the heads (and the salvo landed), she steers west for her point again.
        let shown = until, freed = -1;
        steps(sim, salvo.at - s.t + 1, () => {
          if (freed >= 0) return;
          if (bb.extra.clearFrom !== undefined) shown = bb.extra.clearUntil as number; else freed = s.t;
        });
        expect(freed).toBeGreaterThan(0);
        expect(Math.abs(freed - shown)).toBeLessThanOrEqual(0.2 + 1e-9);
        expect(sim.orderOf(bb.id)).toBe(order);
        expect(bb.extra.clearFrom).toBeUndefined();
        expect(bb.extra.ordered).toBeUndefined();
        steps(sim, 30);
        expect(Math.abs(turnTo(bb.face, -Math.PI / 2))).toBeLessThan(0.35);
        expect(sim.orderOf(bb.id)).toBe(order);
      } else {
        expect(turned).toBe(false);
        expect(leastWest).toBeLessThan(0.05);
        expect(s.tally['att:clear']).toBeUndefined();
        expect(bb.extra.ordered).toBeUndefined();
      }
    });
  }

  /**
   * A destroyer piece the player sends at the enemy battleship, inside its pressing range, with her own battleship lying
   * across the line between: with the rule on it holds its salvo, keeps the order and the run, and the look fields name
   * the ship in the way and say the order stands; moved clear, the salvo goes at once. With the rule off it fires
   * through her.
   */
  for (const on of [true, false]) {
    it(`${on ? 'with the rule on' : 'with the rule off'}, a destroyer piece the player sends at a ship ${on ? 'holds its torpedoes while its own ship is in the way, keeps the order and its run, and fires once the water is clear' : 'fires with its own ship in the way'}`, () => {
      const { sim, s, bb, dd, foe } = field(on);
      put(dd, 5000, 5000, 0, 0);
      put(foe, 5000, 7400, Math.PI / 2, 0);
      put(bb, 5000, 6400, Math.PI / 2, 0);
      sim.command({ do: 'hold', units: [bb.id] }, 'att');
      s.contactAt = s.t;
      expect(sim.command({ do: 'attack', units: [dd.id], target: foe.id }, 'att')).toBe(true);
      const order = sim.orderOf(dd.id);
      const fired = (): boolean => s.torps.some((t: any) => t.from === dd.id);
      let firedAt = -1;
      // Two seconds with her in the way: the piece's own guns and the enemy's are not what is tested; she is put back
      // in the line each tick so their shots cannot move her.
      steps(sim, 2, () => { put(bb, 5000, Math.min(6400, dd.y + 700), Math.PI / 2, 0); if (firedAt < 0 && fired()) firedAt = s.t; });
      if (!on) { expect(firedAt).toBeGreaterThanOrEqual(0); return; }
      expect(firedAt).toBe(-1);
      expect(s.runs.has(dd.id)).toBe(true);
      expect(sim.orderOf(dd.id)).toBe(order);
      expect(s.tally['att:torpHeld']).toBe(1);
      expect(dd.extra.heldFor).toBe(bb.id);
      expect(typeof dd.extra.heldForName).toBe('string');
      expect(dd.extra.ordered).toBe(true);
      // She steams out of the line: the salvo goes the first tick the water is clear, at the ship the player named.
      put(bb, 9000, 6400, Math.PI / 2, 0);
      steps(sim, 1, () => { if (firedAt < 0 && fired()) firedAt = s.t; });
      expect(firedAt).toBeGreaterThan(0);
      expect(s.torps.find((t: any) => t.from === dd.id).target).toBe(foe.id);
      expect(sim.orderOf(dd.id)).toBe(order);
      expect(s.tally['att:torpHeld']).toBe(1);
      steps(sim, 1.5);
      expect(dd.extra.heldFor).toBeUndefined();
      expect(dd.extra.ordered).toBeUndefined();
    });
  }
});
