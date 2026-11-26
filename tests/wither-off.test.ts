// The withering rule with the far switch off (ruling 173(e), rider (o)): the islands' table is not read, nothing
// answers, and a cut-off garrison on any island of today's chart starves exactly as it always did: four tenths of its
// order and a seventh of its strength a month, and its arms laid down in the third. No flag of the rule is written.
import { describe, expect, it } from 'vitest';
import { addCorps, atWar, capture, computeSupply, createGame, declareWar, endPhase, removeCorps, startPhase } from '../src/campaign/game';
import { islandNames, islandOf } from '../src/campaign/islands';
import { corpsMood } from '../src/campaign/morale';
import { closedTo } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { FAR_STAGE } from '../src/campaign/stage';
import { fieldWithered, witherAt, withered } from '../src/campaign/wither';
import { farOn } from '../src/campaign/world';

describe('the withering rule with the far side off', () => {
  it("does not read the islands' table: no island has a row, and nothing answers", () => {
    expect(FAR_STAGE).toBe(false);
    const g = startOpen(createGame(3, null));
    expect(farOn(g)).toBe(false);
    expect(islandNames()).toEqual([]);
    for (const name of ['Rhodes', 'Crete', 'Malta', 'Sardinia', 'Iceland', 'Cyprus']) {
      const r = g.byName[name];
      if (r) expect(islandOf(g, r), name).toBeNull();
    }
    for (const c of g.corps.slice(0, 40)) {
      expect(withered(g, c)).toBe(false);
      expect(witherAt(g, c, false)).toBeNull();
      expect(witherAt(g, c, true)).toBeNull();
    }
    for (const f of g.airFleets.slice(0, 20)) { f.unfed = 1; expect(fieldWithered(g, f)).toBe(false); delete f.unfed; }
  }, 300000);

  it('and an Italian garrison cut off on Rhodes starves and lays down its arms in its third month, as it always did', () => {
    const g = startOpen(createGame(3, null));
    g.turn = 28;
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    g.turn = 28;
    const r = g.byName['Rhodes'];
    if (r.ctrl !== 'ITA') { for (const u of [...r.units]) removeCorps(g, u); capture(g, r, 'ITA'); r.since = 20; }
    const c = addCorps(g, 'ITA', 'Rhodes garrison (a test row)', 'inf', 1, 'Rhodes', {});
    for (const f of g.fleets) { if (f.nat === 'UK') { f.zone = r.seas[0]; f.stance = 'patrol'; } else if (f.nat === 'ITA') f.stance = 'port'; }
    computeSupply(g);
    expect(closedTo(g, r.seas[0], 'ITA')).toBe(true);
    expect(c.supplied).toBe(false);
    // The card says what it always said: no word of stores or of a floor.
    g.human = 'ITA';
    expect(corpsMood(g, c).restLine).toBe('Cut off from supply: it gets no rest at all.');
    const seen: string[] = [];
    for (let m = 1; m <= 3; m++) {
      g.turn++;
      startPhase(g, 'ITA');
      endPhase(g, 'ITA');
      for (const f of g.fleets) if (f.nat === 'ITA') f.stance = 'port';
      seen.push(c.dead ? 'gone' : `${Math.round(c.org * 1e4) / 1e4}/${Math.round(c.str * 1e4) / 1e4} ap ${c.ap}`);
      expect(withered(g, c)).toBe(false);
    }
    expect(seen).toEqual(['0.6/0.85 ap 4', '0.2/0.7225 ap 4', 'gone']);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('wither:'))).toEqual([]);
  }, 300000);
});
