// Air campaigns in a played grand campaign (ruling 236.2; the design director, 22 November 12:28Z, b623 and b636):
// release 1's form of release 2's air-campaigns-on (fb2b66a1), read alike.
//
// Before this, nothing in a played path set g.flags.ac. Only the war runners did (TC_AC=1, right after
// startOpen(createGame(seed, null))), so every gate since 236 measured air campaigns on while the game the player
// plays had them off. As shipped now (aircampaigns.json `on`), a new grand campaign has them, in the browser (newGame)
// and in the 3D game (the campaign bridge's start, on the unreal branch, with its own test). Each sets them right after
// startOpen, as the runners do, so the war started there is the war the gates measured. Scenarios, history campaigns
// and saved games keep what they had.
import { describe, expect, it } from 'vitest';
import page from '../src/campaign/ui/main.ts?raw';
import { AC, acOn, airCampaignsShipped, setAirCampaigns } from '../src/campaign/aircampaigns';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import * as turnAll from '../src/campaign/turn';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { hashString } from '../src/core/rng';

const fp = (g: Game): string => hashString(saveGame(g)).toString(16);

/** The gate runner's start (par6h.ts lines 19 to 22) at stop 3, with TC_AC=1. */
function runnerStart(seed: number): Game {
  const g = startOpen(createGame(seed, null));
  g.flags.ac = 1;
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

  it("the browser's newGame sets them right after the last startOpen, in a grand campaign only", () => {
    const body = page.slice(page.indexOf('function newGame('), page.indexOf('ui.live = true;', page.indexOf('function newGame(')));
    const lines = body.split('\n').map((l) => l.trim()).filter((l) => l && !l.startsWith('//'));
    const at = lines.indexOf('if (grand && airCampaignsShipped()) setAirCampaigns(g, true);');
    expect(at).toBeGreaterThan(0);
    expect(lines[at - 1]).toBe('if (grand && from > 1) startOpen(g);');
    // (Release 1 has no damaged cities: the player's seat is set next.)
    expect(lines[at + 1]).toBe('g.human = human;');
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

  it('a history campaign never has them, and a grand campaign saved without the flag plays on with them off', () => {
    // A history campaign is not open: the browser does not set the switch there, and acOn would not read it.
    const h = createGame(7, 'GER');
    expect(h.flags.ac).toBeUndefined();
    expect(acOn(h)).toBe(false);
    // An old save: a grand campaign started before the switch shipped.
    const old = startOpen(createGame(7, 'GER'));
    const g = loadGame(saveGame(old))!;
    expect(g.flags.open).toBe(1);
    expect(g.flags.ac).toBeUndefined();
    expect(acOn(g)).toBe(false);
    runTurnAI(g);
    expect(acOn(g)).toBe(false);
    // One saved with them on keeps them on.
    const now = startOpen(createGame(7, 'GER'));
    if (airCampaignsShipped()) setAirCampaigns(now, true);
    expect(acOn(loadGame(saveGame(now))!)).toBe(true);
  });
});

describe("a war started as the game starts it is the gate runner's TC_AC=1 war (seed 17, stop 3)", () => {
  // (The whole war, 77 months, is the gate runner's own to play: par6h.ts on this tree against ffb15404, in NOTES.)
  it('plays the same months: the switch set where the runner sets it', { timeout: 600_000 }, () => {
    // The game's way: startOpen, then the shipped switch as the interfaces call it.
    const g = startOpen(createGame(17, null));
    if (airCampaignsShipped()) setAirCampaigns(g, true);
    const r = runnerStart(17);
    expect(fp(g), 'start').toBe(fp(r));
    for (let t = 1; t <= 3; t++) {
      runTurnAI(g); runTurnAI(r);
      expect(fp(g), `month ${t}`).toBe(fp(r));
    }
    expect(acOn(g)).toBe(true);
  });
});
