// Lane (a) of release 2, a raid on troops flown as a battle: the computer's air staff in the grand campaign with the
// lane's switch ON (flyKinds(['ground'])), the twin of the shipped test in tests/airstaff.test.ts "comes to a front only
// where the armies are fighting: a quiet front stays quiet in the air", whose line at :245 keeps pinning the shipped
// words with the lane off (the design director, 26 November 2026 02:06Z: "airstaff.test.ts:245 stays as it is, and there
// is no widened pattern: a pattern that accepts either line is a loosened pin. Instead, add now a lane-on twin that pins
// her new words exactly at Champagne; :245 keeps pinning the shipped words with the lane off. When the lane ships ON, its
// commit says which pin governs.").
//
// Her words (the UI director, 25 November 2026 21:22Z, i): under the lane, where the cut rounds to 0%, "The raids did the
// troops little harm."; where the cut is nothing at all, "The raids did the troops no harm."; off, the base's "The
// aircraft found little to hit." stands as shipped.
//
// The same game set up the same way as the shipped test (its helpers' setup lines copied: the grand campaign, France the
// player, colonel, seed 3; the staff's fight back held off as that test's describe holds it), the lane switched on before
// and restored after. Ten months, as the shipped test plays.
import { afterAll, afterEach, beforeAll, describe, expect, it } from 'vitest';
import { AIRWAR, airAI } from '../src/campaign/air';
import { setLevel } from '../src/campaign/difficulty';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import { flyKinds } from '../src/battle/air/ticket';
import type { AirReport, Game } from '../src/campaign/types';

const HARM_LITTLE = 'The raids did the troops little harm.', HARM_NONE = 'The raids did the troops no harm.';
// (The base's harm line, which the lane leaves as it is where the cut prints above 0%.)
const ORGANISATION = /^The raids cut the organisation of .+ by [1-9]\d*%(, less where they are dug in)?\. They fight the weaker for it until they have rested\.$/;

const grand = (human: string | null = 'FRA', level = 'colonel', seed = 3): Game => { const g = startOpen(createGame(seed, human)); setLevel(g, level); return g; };
/** Plays so many months with the player giving no orders, and returns the raids flown in them. */
function months(g: Game, n: number): AirReport[] {
  const from = g.airReports.length;
  for (let m = 0; m < n; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    if (!humanUp(g)) break;
    beginHuman(g);
    endHuman(g);
  }
  return g.airReports.slice(from);
}

afterEach(() => { flyKinds([]); });

describe('the computer\'s air staff with lane (a) on: the twin of airstaff.test.ts\'s quiet front (the design director, 26 Nov 02:06Z)', () => {
  beforeAll(() => { AIRWAR.ai.back!.on = false; });
  afterAll(() => { AIRWAR.ai.back!.on = true; });

  it('a raid on troops whose harm rounds to nothing reads her words, exactly at Champagne; never "little to hit"', () => {
    flyKinds(['ground']);
    expect(flyKinds()).toEqual(['ground']);
    const g = grand('FRA');
    airAI(g, 'GER');
    const later = months(g, 10);
    const ground = later.filter((x) => x.mission === 'ground');
    // The war itself, so the test fails loudly if it changes: the British raid on the German troops in Champagne,
    // flown in month 8 (its report's turn), the one raid on troops there in these ten months.
    const champagne = ground.filter((r) => r.place === 'Champagne');
    expect(champagne.map((r) => [r.attNat, r.turn])).toEqual([['UK', 8]]);
    expect(champagne[0].result).toBe(HARM_LITTLE);
    // Every raid on troops in those months: the base's harm line, or exactly her little-harm or no-harm sentence.
    expect(ground.length).toBeGreaterThan(0);
    for (const r of ground) {
      expect(ORGANISATION.test(r.result) || r.result === HARM_LITTLE || r.result === HARM_NONE, `${r.place}: ${r.result}`).toBe(true);
      expect(r.result, r.place).not.toContain('little to hit');
    }
  });
});
