// Ruling 234, the design director's note of 10:25Z on rider b630 (historian/sea-lift-record-b630.md): four facts of
// the sea lift. America carries 2 corps a month by sea before December 1941 and 3 from January 1942; its landing craft
// come in 1942, not 1941; Norway's and Greece's merchant fleets give 1, not 2; Denmark's yards give none.
import { describe, expect, it } from 'vitest';
import { createGame, startPhase } from '../src/campaign/game';
import { LIFT, liftDates } from '../src/campaign/lift';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { hasTech, startResearch, techOf, techTick, whyNotResearch } from '../src/campaign/tech';
import type { Game } from '../src/campaign/types';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const at = (g: Game, turn: number): Game => { g.turn = turn; return g; };
function research(g: Game, nat: string, id: string): void {
  techOf(g, nat)!.work = []; // (a free team: the staff's own projects set aside)
  expect(startResearch(g, nat, id), `${nat} begins ${id}: ${whyNotResearch(g, nat, id)}`).toBe(true);
  const months = techOf(g, nat)!.work.find((w) => w.id === id)!.left;
  for (let k = 0; k < months; k++) techTick(g, nat);
  expect(hasTech(g, nat, id), id).toBe(true);
}

describe('sea lift by the record (b630)', () => {
  it('gives America 2 from the first day and 3 from January 1942, once', () => {
    const g = grand();
    expect(g.nations.USA.sealift).toBe(2);
    startPhase(at(g, 28), 'USA');
    expect(g.nations.USA.sealift).toBe(2);
    startPhase(at(g, 29), 'USA');
    expect(g.nations.USA.sealift).toBe(3);
    startPhase(at(g, 30), 'USA');
    liftDates(g, 'USA');
    expect(g.nations.USA.sealift).toBe(3);
    expect(loadGame(saveGame(g))!.nations.USA.sealift).toBe(3);
  });

  it('opens a later campaign with the dated figure already read', () => {
    const g = at(grand(), 40);
    liftDates(g, 'USA');
    expect(g.nations.USA.sealift).toBe(3);
  });

  it('holds America\'s landing craft back to 1942, and gives Britain hers at once', () => {
    const g = at(grand(), 20);
    const usa = g.nations.USA.sealift, uk = g.nations.UK.sealift;
    research(g, 'USA', 'landingcraft');
    research(g, 'UK', 'landingcraft');
    expect(g.nations.UK.sealift).toBe(uk + 1);
    expect(g.nations.USA.sealift).toBe(usa);
    // (The held-back shipping is saved with the game.)
    const h = loadGame(saveGame(g))!;
    startPhase(at(h, 28), 'USA');
    expect(h.nations.USA.sealift).toBe(usa);
    startPhase(at(h, 29), 'USA');
    expect(h.nations.USA.sealift).toBe(usa + 2); // the dated row's 1 and the landing craft's 1
    startPhase(at(h, 30), 'USA');
    expect(h.nations.USA.sealift).toBe(usa + 2);
  });

  it('gives America\'s landing craft at once from 1942', () => {
    const g = at(grand(), 33);
    liftDates(g, 'USA');
    const usa = g.nations.USA.sealift;
    research(g, 'USA', 'landingcraft');
    expect(g.nations.USA.sealift).toBe(usa + 1);
  });

  it('gives Norway and Greece one force by sea and Denmark none', () => {
    const g = grand();
    expect(g.nations.NOR.sealift).toBe(1);
    expect(g.nations.GRE.sealift).toBe(1);
    expect(g.nations.DEN.sealift).toBe(0);
    expect(g.nations.GER.sealift).toBe(4);
    expect(g.nations.UK.sealift).toBe(4);
    expect(g.nations.ITA.sealift).toBe(3);
  });

  it('names its rows in the data', () => {
    expect(LIFT.dated).toEqual([[29, 'USA', 1, expect.any(String)]]);
    expect(LIFT.techFrom).toEqual({ landingcraft: { USA: 29 } });
  });
});
