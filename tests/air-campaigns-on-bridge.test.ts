// Air campaigns in a played grand campaign, in the 3D game (ruling 236.2; the design director, 22 November 12:28Z,
// b623 and b636): the campaign bridge's start, release 1's form of release 2's air-campaigns-on (fb2b66a1). The
// rules' half, and the browser's, is tests/air-campaigns-on.test.ts.
import { describe, expect, it } from 'vitest';
import { acOn } from '../src/campaign/aircampaigns';
import { createGame, ensureSupply } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { hashString } from '../src/core/rng';
import { airfieldTargetsShipped, setAirfieldTargets, setCityDamage } from '../src/campaign/turn';
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
    // A history campaign saved and loaded keeps them off: only a grand campaign is ever switched on at load.
    TC.start({ human: 'GER', seed: 7 });
    TC.load(TC.save().text);
    expect(flagsOf(TC.save().text).ac).toBeUndefined();
    // A grand campaign saved before the switch (no flag) flies them from the month it is loaded in the 3D game (Brian,
    // 23 November: "i want this in release 1"); the browser, the test kitchen, still plays it on as saved.
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    const old = JSON.parse(TC.save().text);
    delete old.flags.ac;
    const text = JSON.stringify(old);
    TC.load(text);
    expect(flagsOf(TC.save().text).ac).toBe(1);
    const g = loadGame(text)!;
    expect(g.flags.open).toBe(1);
    expect(acOn(g)).toBe(false);
  });

  it('an old grand campaign saved in 1941 with air campaigns off flies them once loaded: six months, no error, the air war running', { timeout: 600_000 }, () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 11 });
    const old = JSON.parse(TC.save().text);
    delete old.flags.ac;
    delete old.ac;
    TC.load(JSON.stringify(old));
    const on = JSON.parse(TC.save().text);
    expect(on.flags.ac).toBe(1);
    expect(on.headlines.some((h: { text: string }) => h.text.startsWith('Air campaigns are flown in this war'))).toBe(true);
    for (let m = 0; m < 6; m++) (TC as unknown as { endMonth(): unknown }).endMonth();
    const after = JSON.parse(TC.save().text);
    expect(after.turn).toBe(on.turn + 6);
    expect(after.flags.ac).toBe(1);
    // The air war under them is running: bombing wears the stocks it is kept for (aircampaigns.ts stockLoss). The standing
    // campaigns are the player's to open, and no 3D panel opens one in this release.
    expect(Object.keys(after.ac.stock).length).toBeGreaterThan(0);
  });

  it("the grand start for nobody, on the wider world, is the gate runner's start with TC_AC=1 and TC_DMG=1 and airfield targets (seed 17, stop 3), to the byte", () => {
    TC.start({ view: 'grand', human: null, seed: 17, world: true });
    const bridge = TC.save().text;
    // (par6h.ts lines 19 to 22: startOpen(createGame(seed, null)), then g.flags.ac = 1. In release 2 the bridge's grand
    // start also switches damaged cities on and airfields as targets, as tests/air-campaigns-on.test.ts's runner does.)
    const runner = startOpen(createGame(17, null));
    runner.flags.ac = 1;
    if (!setCityDamage(runner, true)) throw new Error('no city damage');
    if (airfieldTargetsShipped()) setAirfieldTargets(runner, true);
    // (Release 2's supply window, game.ts keepSupply: every call on the bridge's table hands back fresh supply, so the
    // bridge's save carries the supply as drawn; the runner draws it too. Nothing else in the save differs.)
    ensureSupply(runner);
    expect(hashString(bridge).toString(16)).toBe(fp(runner));
    expect(flagsOf(bridge).ac).toBe(1);
  });
});
