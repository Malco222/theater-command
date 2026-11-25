// Air campaigns in a played grand campaign (ruling 236.2; the design director, 22 November 12:28Z, b623 and b636).
//
// Before this, nothing in a played path set g.flags.ac. Only the war runners did (TC_AC=1, right after
// startOpen(createGame(seed, null))), so every gate since 236 measured air campaigns on while the game the player
// plays had them off. As shipped now (aircampaigns.json `on`), a new grand campaign has them, in the browser (newGame)
// and in the 3D game (the campaign bridge's start). Each sets them right after startOpen, as the runners do, so the war
// started there is the war the gates measured. Scenarios, history campaigns and saved games keep what they had.
import { describe, expect, it } from 'vitest';
import page from '../src/campaign/ui/main.ts?raw';
import { AC, acOn, airCampaignsShipped, setAirCampaigns } from '../src/campaign/aircampaigns';
import { createGame, ensureSupply } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import * as turnAll from '../src/campaign/turn';
import { airfieldTargetsShipped, runTurnAI, setAirfieldTargets, setCityDamage } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { hashString } from '../src/core/rng';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

interface Bridge { start(o: Record<string, unknown>): unknown; save(): { text: string }; load(text: string): unknown }
const TC = (globalThis as Record<string, unknown>).TC as Bridge;
const flagsOf = (text: string): Record<string, number> => JSON.parse(text).flags;
const fp = (g: Game): string => hashString(saveGame(g)).toString(16);

/** The gate runner's start (par6h.ts lines 19 to 24) at stop 3, with TC_AC=1 and, where asked, TC_DMG=1. */
function runnerStart(seed: number, dmg: boolean): Game {
  const g = startOpen(createGame(seed, null));
  g.flags.ac = 1;
  if (dmg && !setCityDamage(g, true)) throw new Error('no city damage');
  return g;
}

describe('air campaigns are on in a new grand campaign as shipped (b623, b636)', () => {
  it('ships on, and turn.ts gives the bridge its two names', () => {
    expect(AC.on).toBe(true);
    expect(airCampaignsShipped()).toBe(true);
    const byName = turnAll as unknown as Record<string, unknown>;
    expect(byName.airCampaignsShipped).toBe(airCampaignsShipped);
    expect(byName.setAirCampaigns).toBe(setAirCampaigns);
  });

  it("the browser's newGame sets them right after startOpen, before damaged cities, in a grand campaign only", () => {
    const body = page.slice(page.indexOf('function newGame('), page.indexOf('ui.live = true;', page.indexOf('function newGame(')));
    const lines = body.split('\n').map((l) => l.trim()).filter((l) => l && !l.startsWith('//'));
    const at = lines.indexOf('if (grand && airCampaignsShipped()) setAirCampaigns(g, true);');
    expect(at).toBeGreaterThan(0);
    expect(lines[at - 1]).toBe('if (grand && from > 1) startOpen(g);');
    expect(lines[at + 1]).toBe('if (grand) setCityDamage(g, true);');
    // newGame's grand start for September 1939 and for a later month, with nothing else between startOpen and the switch.
    for (const from of [1, 4]) {
      const g = createGame(7, from > 1 ? null : 'GER');
      if (from === 1) startOpen(g);
      while (g.turn < from) runTurnAI(g);
      if (from > 1) startOpen(g);
      if (airCampaignsShipped()) setAirCampaigns(g, true);
      expect(acOn(g), `from ${from}`).toBe(true);
    }
  });

  it("the bridge's start sets them for a grand campaign and an open one; a history campaign and an old save keep theirs", () => {
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
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
});

describe("a war started as the game starts it is the gate runner's TC_AC=1 war (seed 17, stop 3)", () => {
  it("the bridge's grand start for nobody, on the wider world, is the runner's start with TC_AC=1 and TC_DMG=1, to the byte", () => {
    TC.start({ view: 'grand', human: null, seed: 17, world: true });
    const bridge = TC.save().text;
    const runner = runnerStart(17, true);
    // (Since fe7ea2e5 the bridge also sets airfields as targets last, as the browser's newGame does, where damage.json
    // ships them on: Brian's airfield ask, the playtester 22 November 21:26Z. The runner's start takes the same switch
    // here; whether the gate runners set it too is the gates' own call.)
    if (airfieldTargetsShipped()) setAirfieldTargets(runner, true);
    // (The supply window, 25 November 2026: every call the table makes hands back with supply fresh, its start too.
    // The runner's start leaves supply for its first step to draw, which draws the same picture: drawn here, the two
    // saves are the same to the byte. tests/supply-window-r2.)
    ensureSupply(runner);
    expect(hashString(bridge).toString(16)).toBe(fp(runner));
    expect(flagsOf(bridge).ac).toBe(1);
  });

  // (The whole war, 76 months, is the gate runner's own to play: par6h.ts on this tree against the release-2 base, in NOTES.)
  it('and plays the same months: the switch set where the runner sets it, damaged cities on or off', { timeout: 600_000 }, () => {
    for (const dmg of [false, true]) {
      // The game's way: startOpen, then the shipped switch as the interfaces call it, then damaged cities as they do.
      const g = startOpen(createGame(17, null));
      if (airCampaignsShipped()) setAirCampaigns(g, true);
      if (dmg) setCityDamage(g, true);
      const r = runnerStart(17, dmg);
      expect(fp(g), `dmg ${dmg}, start`).toBe(fp(r));
      for (let t = 1; t <= 3; t++) {
        runTurnAI(g); runTurnAI(r);
        expect(fp(g), `dmg ${dmg}, month ${t}`).toBe(fp(r));
      }
      expect(acOn(g)).toBe(true);
    }
  });
});
