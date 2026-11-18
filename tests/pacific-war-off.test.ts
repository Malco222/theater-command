// The Pacific war's ties with the far side's switch off (ruling 197): nothing of them is in the game. The entry of
// December 1941 stands at its month's opening as it always did, the month's close reads nothing, and the rule that
// lets two seats files give one country rows ("JAP+") is the far-side rig's rule with one more key.
import { describe, expect, it } from 'vitest';
import { plannedStrike } from '../src/campaign/carriers';
import { DIPLOMACY, closeDiplomacy, holds, monthlyDiplomacy, warDiplomacy } from '../src/campaign/diplomacy';
import { FAR_LAID, lay } from '../src/campaign/far-rows';
import { atWar, capitulate, createGame, declareWar, setWar } from '../src/campaign/game';
import { HISTORY } from '../src/campaign/history';
import { NAVY } from '../src/campaign/navy';
import { PACIFIC, demand, demandInstead, freezeAt, keepersOf, keptForces, mayDemand, pacificOpening, pacificSides, pacificWar, pactAt, pactBinds } from '../src/campaign/pacific';
import { FAR_STAGE } from '../src/campaign/stage';
import { ops, startOpen } from '../src/campaign/staff';
import open from '../data/campaign/open.json';

describe('the Pacific war with the far side off', () => {
  it('reads no rule of it and lays no row of it', () => {
    expect(FAR_STAGE).toBe(false);
    expect(FAR_LAID).toEqual([]);
    expect(PACIFIC).toBeNull();
    expect(DIPLOMACY.events.some((e) => e.id === 'pact-binds')).toBe(false);
    expect(DIPLOMACY.events.some((e) => e.when !== undefined || e.says !== undefined || e.then !== undefined)).toBe(false);
  });

  it("brings America in as December 1941 opens, and the month's close reads nothing", () => {
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'UK');
    g.turn = 27;
    const before = JSON.stringify([g.flags, g.headlines.length, g.nations.USA.align]);
    closeDiplomacy(g);
    pacificOpening(g);
    pacificSides(g);
    pacificWar(g, 'GER', 'UK');
    expect(JSON.stringify([g.flags, g.headlines.length, g.nations.USA.align])).toBe(before);
    expect(freezeAt(g)).toBe(0);
    expect(pactAt(g)).toBe(0);
    expect(pactBinds(g, 'JAP', 'USA')).toBe(false);
    expect(holds(g, 'pact:JAP,USA')).toBe(false);
    g.turn = 28;
    monthlyDiplomacy(g);
    expect(g.flags['event:pearl-harbor-world']).toBe(28);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(g.headlines.some((h) => h.turn === 28 && /Pearl Harbor/.test(h.text))).toBe(true);
    // A declaration of war brings nobody else in by the Pacific's ties.
    const wars = g.nations.UK.atWar.length;
    declareWar(g, 'USA', 'ITA');
    expect(g.nations.UK.atWar.length).toBe(wars);
  });
});

describe("Japan's opening with the far side off (rulings 199 and 200)", () => {
  it('has no row of it: no great choice of Japan, no page of the book for the Pacific, no event read at a declaration, no strike or quay on any plan', () => {
    expect(ops('JAP')).toEqual([]);
    expect(HISTORY.theatres.pacific).toBeUndefined();
    expect(HISTORY.marks.some((m) => m.theatre === 'pacific' || m.expect !== undefined)).toBe(false);
    expect(DIPLOMACY.events.some((e) => e.when === 'war' || e.id === 'siam-beside-japan')).toBe(false);
    expect(Object.values(open.ops as Record<string, { strikes?: unknown; quays?: unknown }[]>).flat().some((op) => op.strikes !== undefined || op.quays !== undefined)).toBe(false);
    expect((NAVY.moves ?? []).some((m) => m.fleet === 'Mobile Fleet')).toBe(false);
  });

  it('makes no demand, keeps nothing for a fallen country by the far rule, and a declaration of war reads no event', () => {
    const g = startOpen(createGame(3, null));
    g.turn = 23;
    expect(mayDemand(g)).toBe(false);
    expect(demand(g)).toBe(false);
    expect(demandInstead(g, 'GER', 'FRA')).toBe(false);
    expect(g.regions.every((r) => keepersOf(g, r) === null)).toBe(true);
    expect(keptForces(g, 'FRA', 'GER').size).toBe(0);
    expect(plannedStrike(g, 'UK', 'Home Fleet', 'Kiel')).toBe(false);
    const before = JSON.stringify([g.flags, g.headlines.length]);
    warDiplomacy(g);
    pacificOpening(g);
    expect(JSON.stringify([g.flags, g.headlines.length])).toBe(before);
    // France falls as it always did: the armistice, and its far ground in Britain's keeping with no soldier of France left on it.
    setWar(g, 'GER', 'FRA');
    setWar(g, 'GER', 'UK');
    capitulate(g, 'FRA', 'GER');
    expect(g.nations.VIC.alive).toBe(true);
    expect(g.corps.some((c) => c.nat === 'FRA' && !c.dead)).toBe(false);
    expect(g.flags['pac:demand']).toBeUndefined();
  });
});

describe("the far side's rows: a nation's code with a plus lays over what stands", () => {
  it("replaces whole under a nation's code, as the rig does", () => {
    const base = { ops: { JAP: [{ id: 'a', n: 1 }] }, dates: { JAP: { a: 3 } } };
    lay(base, { ops: { JAP: [{ id: 'b', n: 2 }] }, dates: { JAP: { b: 5 } } });
    expect(base.ops.JAP).toEqual([{ id: 'b', n: 2 }]);
    expect(base.dates.JAP).toEqual({ b: 5 });
  });

  it('lays a list over the list and an object over the object under the code with a plus, the rows first laid first', () => {
    const base: any = { ops: { JAP: [{ id: 'a', n: 1 }] }, dates: { JAP: { a: 3 } }, temper: {} };
    lay(base, { ops: { 'JAP+': [{ id: 'b', n: 2 }, { id: 'a', n: 9 }] }, dates: { 'JAP+': { b: 5 } }, temper: { 'JAP+': { attackAt: 1.2 } } });
    expect(base.ops.JAP).toEqual([{ id: 'a', n: 9 }, { id: 'b', n: 2 }]);
    expect(base.dates.JAP).toEqual({ a: 3, b: 5 });
    // Where nothing stands yet the rows are put down as they are, under the code itself.
    expect(base.temper).toEqual({ JAP: { attackAt: 1.2 } });
    expect('JAP+' in base.ops).toBe(false);
  });
});
