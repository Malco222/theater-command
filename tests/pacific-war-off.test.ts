// The Pacific war's ties with the far side's switch off (ruling 197): nothing of them is in the game. The entry of
// December 1941 stands at its month's opening as it always did, the month's close reads nothing, and the rule that
// lets two seats files give one country rows ("JAP+") is the far-side rig's rule with one more key.
import { describe, expect, it } from 'vitest';
import { plannedStrike } from '../src/campaign/carriers';
import { DIPLOMACY, closeDiplomacy, holds, monthlyDiplomacy, warDiplomacy } from '../src/campaign/diplomacy';
import { FAR_LAID, lay } from '../src/campaign/far-rows';
import { atWar, capitulate, createGame, declareWar, setWar } from '../src/campaign/game';
import { HISTORY, shoreOpen } from '../src/campaign/history';
import { frontArmys, lines, neverHis } from '../src/campaign/line';
import { NAVY, airOver } from '../src/campaign/navy';
import { AIR, airQuality, airUpkeep } from '../src/campaign/air';
import airData from '../data/campaign/air.json';
import { PACIFIC, demand, demandInstead, freezeAt, hopAims, hopShores, hopsBind, keepersOf, keptForces, mayDemand, pacificGround, pacificOpening, pacificSides, pacificWar, pactAt, pactBinds, skiesOn, skiesOver } from '../src/campaign/pacific';
import { FAR_STAGE } from '../src/campaign/stage';
import { freed, ops, startOpen } from '../src/campaign/staff';
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

describe('the months after December 1941 with the far side off (ruling 200, finding 2, and ruling 234(a))', () => {
  it('has no row of them and reads no rule of them: no shores of the Pacific, no line in the south, nobody freed, no shore named', () => {
    expect((HISTORY as { hops?: unknown }).hops).toBeUndefined();
    expect(lines().some((L) => L.id === 'south' || L.lands !== undefined)).toBe(false);
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'UK');
    setWar(g, 'GER', 'FRA');
    g.turn = 40;
    expect(hopsBind(g)).toBe(false);
    for (const nat of ['GER', 'UK', 'USA', 'ITA', 'FRA']) {
      expect(hopShores(g, nat), nat).toBeNull();
      expect(hopAims(g, nat), nat).toEqual([]);
      expect(freed(g, nat), nat).toEqual([]);
    }
    expect(g.regions.some((r) => pacificGround(g, r) || neverHis(g, r) || frontArmys(g, 'GER', r) || frontArmys(g, 'UK', r))).toBe(false);
    // A flag left by a far-side war names no shore here.
    g.flags['st:hop:UK:0'] = g.byName['Calais'].id + 1;
    g.flags['st:hopQ:UK:0'] = g.byName['Kent'].id + 1;
    expect(hopAims(g, 'UK')).toEqual([]);
    // The second front's own rule is asked as it always was: its fourth word is never true here.
    expect(shoreOpen(g, 'FRA', false, false)).toBe(shoreOpen(g, 'FRA', false));
    // And no great choice of anybody's frees a corps.
    expect(Object.values(open.ops as Record<string, { frees?: unknown; freesAfter?: unknown }[]>).flat().some((op) => op.frees !== undefined || op.freesAfter !== undefined)).toBe(false);
  });
});

describe('the Pacific\'s air forces with the far side off (rulings 184 and 187)', () => {
  it('lays no row of them, counts the air over every sea from its coasts, and keeps no group\'s record of feeding', () => {
    // air.json as it is on disk: thirty-six commands, none of Japan's, no nation row for it, no quality by date.
    expect(AIR.fleets.length).toBe(36);
    expect(AIR.fleets.some((r) => r.nat === 'JAP' || 'moves' in r || 'ally' in r || 'arrives' in r || 'quality' in r || 'bmrRange' in r)).toBe(false);
    expect(AIR.nations.JAP).toBeUndefined();
    expect(AIR.production.JAP).toBeUndefined();
    expect(AIR.decline).toBeUndefined();
    expect(JSON.stringify(AIR.fleets)).toBe(JSON.stringify((airData as unknown as { fleets: unknown[] }).fleets));
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'UK');
    setWar(g, 'GER', 'FRA');
    expect(skiesOn(g)).toBe(false);
    for (const z of g.seas) expect(skiesOver(g, z.id), z.key).toBeNull();
    // The coasts decide: Britain's whole air force stands over the Channel, as it always has.
    const channel = g.seas.find((z) => z.key === 'channel')!;
    expect(airOver(g, channel.id, 'GER', true)).toBeGreaterThan(0);
    // A month in the air marks no field as unfed, raises no command on an ally's ground, stands none down, and moves none by the record.
    const before = g.airFleets.map((f) => `${f.nat}|${f.name}|${f.base}`);
    for (let t = 2; t <= 6; t++) { g.turn = t; for (const nat of ['GER', 'UK', 'FRA', 'USA']) airUpkeep(g, nat); }
    expect(g.airFleets.map((f) => `${f.nat}|${f.name}|${f.base}`)).toEqual(before);
    expect(g.airFleets.some((f) => f.unfed !== undefined || f.raised !== undefined)).toBe(false);
    expect(Object.keys(g.flags).some((k) => k.startsWith('air:mv:'))).toBe(false);
    // A nation's quality is its row's at every stop: no date moves it.
    const lw = g.airFleets.find((f) => f.nat === 'GER')!;
    g.turn = 65;
    expect(airQuality(lw, g)).toBe(AIR.nations.GER.quality);
    expect(airQuality(lw)).toBe(AIR.nations.GER.quality);
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
