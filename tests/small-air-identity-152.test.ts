// Release 1.5.2's gate (the design director, 25 November 2026, 03:54Z): decision identity for air arms at or above
// air.json sky.least. The mend for small air arms (airwar.json ai.back.small) must not change a single order of an arm
// that can hold a sky. A whole war cannot be the same with it on and off, since small arms now fight and that changes
// the wars around them, so the same state is fed to the staff twice: states from wars nobody plays (two seeds, early
// and late months), each copied, and every nation whose land fighters are at or above sky.least at war runs its air
// month (its staff's orders, then its campaigns' raids) on one copy with the mend off and on the other with it on.
// Everything the staff decided must come out the same: every air group's mission, ground support, base, escort and
// target, the raids flown with their dice, and the rest of the war's state besides.
import { afterEach, describe, expect, it } from 'vitest';
import { AIR, AIRWAR, airAI, airBattles, landFighters } from '../src/campaign/air';
import { phaseFor } from '../src/campaign/ai';
import { atWar, createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirReport, Game } from '../src/campaign/types';

const SM = AIRWAR.ai.back!.small!;
afterEach(() => { SM.on = true; });

/** The staff's air month for one nation on a copy of the state, with the mend off or on: the whole state after it. */
function staffMonth(g: Game, nat: string, on: boolean): { h: Game; flown: AirReport[] } {
  SM.on = on;
  try {
    const h = loadGame(saveGame(g))!;
    // (a saved war keeps only its recent air reports: the month's raids are those filed after this)
    const n0 = h.airReports.length;
    const planned = phaseFor(h, nat);
    const ph = planned?.ifAtWar && !atWar(h, planned.ifAtWar[0], planned.ifAtWar[1]) ? null : planned;
    airAI(h, nat, ph);
    airBattles(h, nat);
    return { h, flown: h.airReports.slice(n0) };
  } finally { SM.on = true; }
}

/** What a staff decided, group by group, for a readable failure; the whole state is compared after. */
const orders = (h: Game, nat: string) => h.airFleets.filter((f) => f.nat === nat).map((f) => ({ id: f.id, mission: f.mission, target: f.target, close: !!f.close, base: f.base, escorts: f.escorts ?? null, flew: f.flew ?? null, moved: f.moved, ftr: f.ftr, bmr: f.bmr, ready: f.ready }));

/** Every arm at or above sky.least at war, checked; returns how many nations were and how much they decided. */
function same(g: Game): { nations: number; decided: number } {
  let nations = 0, decided = 0;
  for (const nat of Object.keys(g.nations)) {
    if (!g.nations[nat].alive || !g.nations[nat].atWar.length || landFighters(g, nat) < AIR.sky.least) continue;
    const off = staffMonth(g, nat, false), on = staffMonth(g, nat, true);
    expect(orders(on.h, nat), `${nat} in month ${g.turn}`).toEqual(orders(off.h, nat));
    expect(on.flown, `${nat}'s raids in month ${g.turn}`).toEqual(off.flown);
    expect(saveGame(on.h) === saveGame(off.h), `${nat}'s whole state in month ${g.turn}`).toBe(true);
    nations++;
    decided += on.flown.length + on.h.airFleets.filter((f) => f.nat === nat && f.close).length;
  }
  return { nations, decided };
}

for (const seed of [17, 23]) {
  describe(`release 1.5.2: arms at or above sky.least get the same orders with the mend off and on (seed ${seed})`, () => {
    const g = startOpen(createGame(seed, null));
    g.flags.ac = 1;
    let seen = { nations: 0, decided: 0 };
    // Six stretches of eight months, to month 48 (late 1943): each plays its months and checks two of them.
    for (let k = 0; k < 6; k++) {
      it(`months ${k * 8 + 1} to ${k * 8 + 8}`, () => {
        for (let t = 0; t < 8; t++) {
          runTurnAI(g);
          if (t % 4 === 3) { const s = same(g); seen = { nations: seen.nations + s.nations, decided: seen.decided + s.decided }; }
        }
        expect(seen.nations).toBeGreaterThan(0);
      }, 30_000);
    }
    it('the staffs checked decided something', () => {
      expect(seen.nations).toBeGreaterThanOrEqual(30);
      expect(seen.decided).toBeGreaterThan(0);
    });
  });
}
