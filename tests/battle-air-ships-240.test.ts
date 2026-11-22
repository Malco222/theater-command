// Strikes on ships fought on the field (ruling 238) under ruling 240's switches, joined on release 2 (step 9 of the
// rules join; the design director, 22 November 13:38Z): with damage control on, the field sinks each fleet at its own
// eased share, as fly()'s dice do. The switches are flipped in memory and put back after every test.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { shipHitsBy } from '../src/campaign/air';
import { syncWings } from '../src/campaign/carriers';
import { createGame, setWar } from '../src/campaign/game';
import { sail } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { SHIP_RESEARCH, dcOf, techOf } from '../src/campaign/tech';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, ShipClass } from '../src/campaign/types';
import { ShipBook, type ShipPlace } from '../src/battle/air/ships';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import { Rng } from '../src/core/rng';

const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const R = SHIP_RESEARCH()!;
let was = { stage: false, vt: false, dc: false };
beforeEach(() => { was = { stage: R.stage.on, vt: R.vt.on, dc: R.dc.on }; });
afterEach(() => { R.stage.on = was.stage; R.vt.on = was.vt; R.dc.on = was.dc; });
const set = (stage: boolean, vt: boolean, dc: boolean): void => { R.stage.on = stage; R.vt.on = vt; R.dc.on = dc; };
/** Puts researches straight onto a nation's list, whatever the switches say (tests/ship-aa-240.test.ts). */
const give = (g: Game, nat: string, ...ids: string[]): void => { const s = techOf(g, nat)!; for (const id of ids) if (!s.done.includes(id)) s.done.push(id); };

/** A grand campaign with Britain at the table, played until Britain is at war with Germany (tests/battle-air-ships.test.ts). */
function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

/**
 * The Home Fleet with six carriers off the German Battle Fleet's harbour, and the Italian 2nd Fleet berthed beside the
 * Germans (Italy at war with Britain): a harbour of two navies (tests/ship-aa-240.test.ts raid and berth).
 */
const TWO = (() => {
  const g = britain();
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet'), ally = named(g, '2nd Fleet');
  home.ships.cv = 6;
  sail(g, foe, foe.home, 'port');
  setWar(g, 'UK', ally.nat);
  ally.base = foe.base; ally.stance = 'port';
  home.zone = g.seas.find((z) => z.coast.includes(foe.base))!.id; home.stance = 'patrol'; syncWings(g);
  return saveGame(g);
})();

/** The raid on that harbour, ticketed as a strike on ships ({ ships: true }), on the game as set and then as `prep` leaves it. */
function harbourRaid(prep?: (g: Game) => void): { g: Game; P: ShipPlace; port: number } {
  const g = loadGame(TWO)!;
  prep?.(g);
  const port = named(g, 'Battle Fleet').base;
  const { ticket, why } = airTicket(g, 'harbour', port, raidGroups(g, 'harbour', port, 'UK'), 'UK', { ships: true });
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { g, P: ticket.place.ships!, port };
}

/** What the field sinks of each fleet and class on these throws, by ShipBook's own rule, from expectations by fleet. */
function onThrows(P: ShipPlace, seed: number, e: (i: number, c: ShipClass) => number): Record<number, Partial<Record<ShipClass, number>>> {
  const rng = new Rng(seed), out: Record<number, Partial<Record<ShipClass, number>>> = {};
  P.law.fleets.forEach((f, i) => {
    for (const c of CLASSES) {
      if (f.ships[c] <= 0) continue;
      const u = rng.next(), x = e(i, c);
      (out[f.id] ??= {})[c] = x > u ? Math.min(f.ships[c], Math.floor(x - u) + 1) : 0;
    }
  });
  return out;
}

describe('ruling 240 on the field: a harbour of two navies, damage control on (the design director, 13:38Z)', () => {
  it("sinks each fleet at its own eased share, shipHitsBy's `by`, as fly()'s dice do; not a head count's part of the eased total", () => {
    set(false, false, true);
    // Germany holds Damage control, Italy does not.
    const { g, P: again } = harbourRaid((h) => give(h, 'GER', 'damagecontrol'));
    const L = again.law, ger = L.fleets.findIndex((f) => f.nat === 'GER'), ita = L.fleets.findIndex((f) => f.nat === 'ITA');
    expect(ger).toBeGreaterThanOrEqual(0);
    expect(ita).toBeGreaterThanOrEqual(0);
    expect(L.tough![ger]).toBe(dcOf(g, 'GER'));
    expect(L.tough![ger]).toBeGreaterThan(1);
    expect(L.tough![ita]).toBe(1);
    // A class both navies have: the Germans' own share is less than a head count's part of the eased total, the
    // Italians' more (theirs is the plain law's, uneased).
    const big = shipHitsBy(L, 400 * again.mul);
    const both = CLASSES.filter((c) => c !== 'ss' && L.fleets[ger].ships[c] > 0 && L.fleets[ita].ships[c] > 0);
    expect(both.length).toBeGreaterThan(0);
    for (const c of both) {
      const head = (i: number): number => big.sunk[c] * (L.fleets[i].ships[c] / L.have[c]);
      expect(big.by![ger][c]).toBeLessThan(head(ger));
      expect(big.by![ita][c]).toBeGreaterThan(head(ita));
      expect(big.by![ita][c]).toBeCloseTo(big.plain![c] * (L.fleets[ita].ships[c] / L.have[c]), 12);
    }
    // Box by box, on several sets of throws, the field's sinkings are the ones each fleet's own share gives.
    let sank = 0;
    for (let k = 0; k < 12; k++) {
      const book = new ShipBook(again, new Rng(1000 + k), new Rng(2000 + k), 400);
      for (let d = 25; d <= 400; d += 25) {
        book.drop(d, 'bomb', d);
        const exp = shipHitsBy(L, d * again.mul);
        expect(book.sunk(), `throws ${k}, ${d} dropped`).toEqual(onThrows(again, 1000 + k, (i, c) => exp.by ? exp.by[i][c] : exp.sunk[c] * (L.fleets[i].ships[c] / L.have[c])));
      }
      sank += Object.values(book.sunk()).reduce((t, x) => t + CLASSES.reduce((n, c) => n + (x[c] ?? 0), 0), 0);
    }
    expect(sank).toBeGreaterThan(0);
  });

  it('with the switch off, or no fleet holding the research, the head count as before: no `by`, the plain law', () => {
    for (const [dc, held] of [[false, true], [true, false]] as const) {
      set(false, false, dc);
      const { P } = harbourRaid((h) => { if (held) give(h, 'GER', 'damagecontrol'); });
      expect(P.law.tough).toBeUndefined();
      const book = new ShipBook(P, new Rng(77), new Rng(78), 400);
      book.drop(400, 'bomb', 1);
      const exp = shipHitsBy(P.law, 400 * P.mul);
      expect(exp.by).toBeUndefined();
      expect(book.sunk()).toEqual(onThrows(P, 77, (i, c) => exp.sunk[c] * (P.law.fleets[i].ships[c] / P.law.have[c])));
    }
  });
});
