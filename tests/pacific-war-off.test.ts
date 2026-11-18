// The Pacific war's ties with the far side's switch off (ruling 197): nothing of them is in the game. The entry of
// December 1941 stands at its month's opening as it always did, the month's close reads nothing, and the rule that
// lets two seats files give one country rows ("JAP+") is the far-side rig's rule with one more key.
import { describe, expect, it } from 'vitest';
import { DIPLOMACY, closeDiplomacy, holds, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { FAR_LAID, lay } from '../src/campaign/far-rows';
import { atWar, createGame, declareWar, setWar } from '../src/campaign/game';
import { PACIFIC, freezeAt, pacificOpening, pacificSides, pacificWar, pactAt, pactBinds } from '../src/campaign/pacific';
import { FAR_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';

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
