// Rearguards blow the bridges (release 2, rearguard.ts rearBridgesOn, rivers.ts bridgesOnCapture; Brian 24 November 2026
// 19:47Z, "yes to all"): a rearguard's charges are laid, so when the province it held is lost the bridges behind it go up
// on REARGUARD.bridges in place of the usual chance. The same draw decides it. Its own switch, read only with rearguards on.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { Rng, hashString } from '../src/core/rng';
import { createGame } from '../src/campaign/game';
import { REARGUARD, chargesLaid, rearBridgesOn, setRearBridges, setRearguard } from '../src/campaign/rearguard';
import { blowChance, bridgeDown, bridgesOnCapture, crossingsOf, type Crossing } from '../src/campaign/rivers';
import { loadGame, saveGame } from '../src/campaign/save';
import { REARBRIDGES_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

function germany(): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

/**
 * A river between two Polish provinces, and a month, where the German taking the near bank would find the bridges
 * standing on the usual chance but blown on a rearguard's charges: the draw falls between the two.
 */
function between(g: Game): { c: Crossing; turn: number } {
  for (let k = 0; k < 40; k++) {
    const turn = g.turn + k;
    for (const c of crossingsOf(g)) {
      if (g.regions[c.a].ctrl !== 'POL' || g.regions[c.b].ctrl !== 'POL' || bridgeDown(g, c)) continue;
      // Only the one river at the near bank, so no other crossing of it muddies the test.
      if (crossingsOf(g).filter((x) => x.a === c.a || x.b === c.a).length !== 1) continue;
      const roll = new Rng(hashString(`bridge:${g.seed}:${turn}:${c.a}:${c.b}`)).next();
      if (roll >= blowChance(g, c, g.regions[c.a], 'GER') && roll < REARGUARD.bridges) return { c, turn };
    }
  }
  throw new Error('no crossing found');
}

describe('rearguards blow the bridges', () => {
  it('ships off', () => {
    expect((stage as { rearBridges?: boolean }).rearBridges).toBe(false);
    expect(REARBRIDGES_STAGE).toBe(false);
    const g = germany();
    expect(rearBridgesOn(g)).toBe(false);
  });

  it("a rearguard's laid charges blow the bridges where the usual chance would not; off, or with no rearguard, they stand", () => {
    const g = germany();
    const { c, turn } = between(g);
    const pole = g.corps.find((x) => x.nat === 'POL' && !x.dead)!;
    const run = (rear: boolean, rule: boolean, guards: boolean): { down: boolean; news: string[] } => {
      const h = loadGame(saveGame(g))!;
      h.turn = turn;
      setRearguard(h, guards);
      setRearBridges(h, rule);
      if (rear) h.corps.find((x) => x.id === pole.id)!.rear = [turn, 0, c.a];
      const n = h.headlines.length;
      bridgesOnCapture(h, h.regions[c.a], 'GER');
      return { down: bridgeDown(h, c), news: h.headlines.slice(n).map((x) => x.text) };
    };
    const none = run(false, true, true);
    expect(none.down).toBe(false);
    const laid = run(true, true, true);
    expect(laid.down).toBe(true);
    expect(laid.news.some((t) => /Polish rearguard at .* blows the bridges over the .* behind it/.test(t))).toBe(true);
    // The switch off, or rearguards off: the usual chance, and the bridges stand.
    expect(run(true, false, true).down).toBe(false);
    expect(run(true, true, false).down).toBe(false);
  });

  it('charges are laid only by a rearguard of the far bank\'s side, in the province it held', () => {
    const g = germany();
    setRearguard(g, true);
    setRearBridges(g, true);
    const pole = g.corps.find((x) => x.nat === 'POL' && !x.dead)!;
    const ger = g.corps.find((x) => x.nat === 'GER' && !x.dead)!;
    pole.rear = [g.turn, 0, pole.reg];
    expect(chargesLaid(g, pole.reg, (n) => n === 'POL')).toBe(true);
    expect(chargesLaid(g, pole.reg, (n) => n === 'GER')).toBe(false);
    expect(chargesLaid(g, ger.reg === pole.reg ? -1 : ger.reg, (n) => n === 'POL')).toBe(false);
  });
});
