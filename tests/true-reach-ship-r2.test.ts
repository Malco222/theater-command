// True reach on the ship list (game.ts trueReach, setTrueReach; the bridge's R2_SHIP 'truereachrule'). Option (b): the
// rule runs on the old chart by its own flag, with Units decide or without it, and the 3D's ship list switches it.
// It is taken only as a September 1939 war opens: kept stores filled before the switch carry no record of whose they
// are (`stores:by:`), and under true reach such stores feed nobody (keptStoresHis), so a war already under way, and a
// later start (its history months were filled with the rule off), are left as they are. The row is marked
// (r2:truereachrule) either way, so the war never asks again.
import { describe, expect, it } from 'vitest';
import { computeSupply, createGame, setTrueReach, trueReach } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { unitsDecide } from '../src/campaign/units';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const fromSave = (): Game => loadGame((TC().save() as { text: string }).text)!;
/** The ship list as gates/gamestart.ts SHIP_PLAY names it (play-a15b57beb, 17 rows), without true reach. */
const PLAY = ['nicknames', 'marked', 'taskforcerule', 'rearguardrule', 'rearbridgesrule', 'shadowrule', 'voyagelogrule',
  'seaminesrule', 'heavyseasrule', 'shadowrelayrule', 'fleetwhyrule', 'settledtownsrule', 'columns',
  'logistics:lorryShift', 'logistics:wagons', 'logistics:dumps', 'seekoncerule'];

describe("true reach on the ship list ('truereachrule')", () => {
  it('off: the list without it lays neither the flag nor the mark, and the war reads as without the rule', () => {
    TC().load(saveGame(startOpen(createGame(1, null))));
    TC().r2Ship(PLAY);
    const g = fromSave();
    expect([unitsDecide(g), trueReach(g), g.flags.trueReach, g.flags['r2:truereachrule']]).toEqual([false, false, undefined, undefined]);
  });

  it('in the opening month, without Units decide: the row takes, once; the save carries the flag and the mark', () => {
    TC().load(saveGame(startOpen(createGame(1, null))));
    expect(TC().r2Ship(['truereachrule'])).not.toBeNull();
    const g = fromSave();
    expect([unitsDecide(g), trueReach(g), g.flags.trueReach, g.flags['r2:truereachrule']]).toEqual([false, true, 1, 1]);
    // (Asked again, nothing: the mark stands.)
    expect(TC().r2Ship(['truereachrule'])).toBeNull();
  });

  it('in a war already under way: the row is refused and marked, the war is left as it is, and it never asks again', () => {
    const w = startOpen(createGame(1, null));
    runTurnAI(w);
    expect(w.turn).toBe(2);
    TC().load(saveGame(w));
    expect(TC().r2Ship(['truereachrule'])).toBeNull();
    const g = fromSave();
    expect([trueReach(g), g.flags.trueReach, g.flags['r2:truereachrule']]).toEqual([false, undefined, 1]);
    expect(TC().r2Ship(['truereachrule'])).toBeNull();
    expect(fromSave().flags.trueReach).toBeUndefined();
  });

  it('outside a grand campaign: nothing', () => {
    TC().load(saveGame(createGame(1, null)));
    expect(TC().r2Ship(['truereachrule'])).toBeNull();
    expect(fromSave().flags.trueReach).toBeUndefined();
  });

  it('switched in the opening month, after the first picture, the war is the war opened with the flag (the whole save, three months)', () => {
    // (staff.ts lays the stage's flag before startOpen draws its first picture; the ship list lays it after. Under true
    // reach a fill writes its record beside its month, so a month mark without a record feeds nobody: such marks are
    // the only thing the order may leave behind, and none is expected on the first day at peace.)
    const A = createGame(7, null); A.flags.trueReach = 1; startOpen(A);
    const B = startOpen(createGame(7, null)); expect(setTrueReach(B, true)).toBe(true);
    const bare = (g: Game) => {
      const d = JSON.parse(saveGame(g));
      d.flags = Object.fromEntries(Object.entries(d.flags as Record<string, number>).sort(([a], [b]) => (a < b ? -1 : 1)));
      return JSON.stringify(d);
    };
    for (const g of [A, B]) computeSupply(g);
    expect(Object.keys(B.flags).filter((k) => /^stores:\d+$/.test(k) && B.flags[`stores:by:${k.slice(7)}`] === undefined)).toEqual([]);
    expect(bare(B)).toBe(bare(A));
    for (let m = 0; m < 3; m++) { runTurnAI(A); runTurnAI(B); expect(bare(B)).toBe(bare(A)); }
  });

  it('a later start is left as it is: the row is refused and marked (its history months were filled with the rule off)', () => {
    TC().start({ view: 'grand', human: null, seed: 3, from: 13 });
    const before = fromSave();
    expect([before.flags.open, before.flags.openFrom]).toEqual([1, before.turn]);
    expect(before.turn).toBeGreaterThan(1);
    expect(setTrueReach(before, true)).toBe(false);
    expect(TC().r2Ship(['truereachrule'])).toBeNull();
    const g = fromSave();
    expect([trueReach(g), g.flags.trueReach, g.flags['r2:truereachrule']]).toEqual([false, undefined, 1]);
  });

  it("through the bridge's own start on the world chart, nobody seated and a country seated: the row takes on the first day with the whole list", () => {
    for (const human of [null, 'UK', 'ITA']) {
      TC().start({ view: 'grand', human, seed: 5, world: true });
      expect(TC().r2Ship([...PLAY, 'truereachrule'])).not.toBeNull();
      const g = fromSave();
      expect([human, g.turn, unitsDecide(g), trueReach(g), g.flags.trueReach, g.flags['r2:truereachrule']]).toEqual([human, 1, false, true, 1, 1]);
    }
  });
});
