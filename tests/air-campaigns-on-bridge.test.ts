// Air campaigns in a played grand campaign, in the 3D game (ruling 236.2; the design director, 22 November 12:28Z,
// b623 and b636): the campaign bridge's start, release 1's form of release 2's air-campaigns-on (fb2b66a1). The
// rules' half, and the browser's, is tests/air-campaigns-on.test.ts.
import { describe, expect, it } from 'vitest';
import { acOn } from '../src/campaign/aircampaigns';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { hashString } from '../src/core/rng';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

interface Bridge { start(o: Record<string, unknown>): unknown; save(): { text: string }; load(text: string): unknown }
const TC = (globalThis as Record<string, unknown>).TC as Bridge;
const flagsOf = (text: string): Record<string, number> => JSON.parse(text).flags;
const fp = (g: Game): string => hashString(saveGame(g)).toString(16);

describe("the 3D game's start: air campaigns on in a new grand campaign as shipped (b623, b636)", () => {
  it("sets them for a grand campaign and an open one; a history campaign and an old save keep theirs", () => {
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    expect(flagsOf(TC.save().text).ac).toBe(1);
    TC.start({ view: 'grand', human: 'UK', from: 13, seed: 7 });
    expect(flagsOf(TC.save().text).ac).toBe(1);
    TC.start({ view: 'open', human: 'GER', seed: 7 });
    expect(flagsOf(TC.save().text).ac).toBe(1);
    TC.start({ human: 'GER', seed: 7 });
    expect(flagsOf(TC.save().text).ac).toBeUndefined();
    // A grand campaign saved before the switch (no flag) plays on with air campaigns off, in the bridge and the browser.
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    const old = JSON.parse(TC.save().text);
    delete old.flags.ac;
    const text = JSON.stringify(old);
    TC.load(text);
    expect(flagsOf(TC.save().text).ac).toBeUndefined();
    const g = loadGame(text)!;
    expect(g.flags.open).toBe(1);
    expect(acOn(g)).toBe(false);
  });

  it("the grand start for nobody, on the wider world, is the gate runner's start with TC_AC=1 (seed 17, stop 3), to the byte", () => {
    TC.start({ view: 'grand', human: null, seed: 17, world: true });
    const bridge = TC.save().text;
    // (par6h.ts lines 19 to 22: startOpen(createGame(seed, null)), then g.flags.ac = 1.)
    const runner = startOpen(createGame(17, null));
    runner.flags.ac = 1;
    expect(hashString(bridge).toString(16)).toBe(fp(runner));
    expect(flagsOf(bridge).ac).toBe(1);
  });
});
