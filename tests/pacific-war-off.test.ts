// The Pacific war's ties with the far side's switch off (ruling 197): nothing of them is in the game. The entry of
// December 1941 stands at its month's opening as it always did, the month's close reads nothing, and the rule that
// lets two seats files give one country rows ("JAP+") is the far-side rig's rule with one more key.
import { describe, expect, it } from 'vitest';
import { answering, cityBound, effortKeeps, effortSends, planSkies } from '../src/campaign/ai';
import { plannedStrike } from '../src/campaign/carriers';
import { DIPLOMACY, closeDiplomacy, holds, monthlyDiplomacy, warDiplomacy } from '../src/campaign/diplomacy';
import { FAR_LAID, lay } from '../src/campaign/far-rows';
import { addCorps, ashoreFor, atWar, capitulate, createGame, declareWar, setWar } from '../src/campaign/game';
import { HISTORY, setStop as setStopOff, shoreOpen } from '../src/campaign/history';
import { frontArmys, lines, neverHis } from '../src/campaign/line';
import { NAVY, airOver, assaultStrength, atSea, landCorps, liftLeft, navalUpkeep, shipsFor, whyNotLand } from '../src/campaign/navy';
import { AIR, airQuality, airRange, airUpkeep } from '../src/campaign/air';
import { ARSENAL, smallAirMonth } from '../src/campaign/arsenal';
import airData from '../data/campaign/air.json';
import { PACIFIC, demand, demandInstead, freezeAt, hopAims, hopShores, hopsBind, keepersOf, keptForces, mayDemand, pacificGround, pacificOpening, pacificSides, pacificWar, pactAt, pactBinds, readyRules, shoreLift, skiesOn, skiesOver } from '../src/campaign/pacific';
import { owedNotBefore } from '../src/campaign/pacific';
import { finishMonthEnd, warNotices } from '../src/campaign/finish';
import { gatherMonth, readiness, readinessMonth } from '../src/campaign/readiness';
import { FAR_STAGE } from '../src/campaign/stage';
import { freed, ops, startOpen } from '../src/campaign/staff';
import { alongside, bookAttacks, bookFleets, bookStations, ceilinged, goesWith, ridersOf, riding, stations, stationsBind } from '../src/campaign/stations';
import open from '../data/campaign/open.json';
import setup from '../data/campaign/setup-1939.json';
import siege from '../data/campaign/siege.json';

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

  it('holds no war to a month of the book: by the book too, the finish rule\'s three months (the director\'s 07:18Z of 22 November 2026)', () => {
    for (const stop of [1, 3]) {
      const g = startOpen(createGame(3, null));
      if (stop !== 3) setStopOff(g, stop, true);
      g.turn = 57;
      // (The old chart has no Japan: the Soviet Union as an ally at war with Germany, and Britain at war with Italy.)
      expect(g.nations.JAP).toBeUndefined();
      g.nations.SOV.align = 'allies';
      setWar(g, 'SOV', 'GER');
      setWar(g, 'UK', 'GER');
      setWar(g, 'UK', 'ITA');
      for (const on of ['JAP', 'ITA', 'GER']) expect(owedNotBefore(g, 'SOV', on)).toBe(0);
      capitulate(g, 'GER', 'SOV');
      finishMonthEnd(g);
      expect(warNotices(g).find((n) => n.who === 'SOV' && n.on === 'ITA'), `stop ${stop}`).toEqual({ who: 'SOV', on: 'ITA', given: 57, due: 60 });
    }
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
    // The second front's own rule is asked as it always was: its last word (the Pacific's) is never true here.
    expect(shoreOpen(g, 'FRA', false, undefined, false)).toBe(shoreOpen(g, 'FRA', false));
    // And no great choice of anybody's frees a corps.
    expect(Object.values(open.ops as Record<string, { frees?: unknown; freesAfter?: unknown }[]>).flat().some((op) => op.frees !== undefined || op.freesAfter !== undefined)).toBe(false);
  });
});

describe('the Pacific\'s air forces with the far side off (rulings 184 and 187)', () => {
  it('lays no row of them, counts the air over every sea from its coasts, and keeps no group\'s record of feeding', () => {
    // air.json as it is on disk: thirty-six commands, none of Japan's, no nation row for it, no quality by date.
    expect(AIR.fleets.length).toBe(36);
    expect(AIR.fleets.some((r) => r.nat === 'JAP' || 'moves' in r || 'ally' in r || 'arrives' in r || 'quality' in r || 'bmrRange' in r || 'byBook' in r || 'watch' in r || 'posted' in r || 'to' in r || 'leaves' in r)).toBe(false);
    expect(AIR.nations.JAP).toBeUndefined();
    expect(AIR.production.JAP).toBeUndefined();
    expect(AIR.decline).toBeUndefined();
    expect([AIR.rise, AIR.jobs, AIR.works]).toEqual([undefined, undefined, undefined]);
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
    // A fighter reaches as its month allows whatever it is doing: no job column (ruling 187(a) is the far side's).
    const us = g.airFleets.find((f) => f.nat === 'USA' && f.ship === undefined);
    if (us) for (const m of ['bomb', 'town', 'ground']) expect(airRange(us, g, m)).toEqual(airRange(us, g));
  });

  it('the corrections of ruling 234\'s rider b518 are the far side\'s: aircraft from abroad come as ruling 213(d) has them, and no works feeds a pool', () => {
    const g = startOpen(createGame(3, null));
    const G = ARSENAL.smallAir.gifts;
    const nz = G.findIndex((r) => r[0] === 24 && r[1] === 'NZL');
    expect(nz).toBeGreaterThanOrEqual(0);
    g.turn = 36;
    smallAirMonth(g);
    for (const nat of Object.keys(ARSENAL.smallAir.works)) expect((g.airPool[nat]?.ftr ?? 0) + (g.airPool[nat]?.bmr ?? 0), nat).toBe(0);
    // (New Zealand is on the map only where the chart reaches it: its row waits for it otherwise, and is never thrown away.)
    if (g.nations.NZL?.alive && !g.nations.NZL.dormant) expect(g.airFleets.some((f) => f.nat === 'NZL')).toBe(true);
    else expect(g.flags[`gift:${nz}`]).toBeUndefined();
  });
});

describe('the carriers of ruling 234(d) with the far side off', () => {
  it('no fleet has a cruise or a month at sea by the record, and a country at peace keeps every fleet in harbour', () => {
    expect(NAVY.fleets.some((r) => 'cruise' in r)).toBe(false);
    expect((NAVY.moves ?? []).some((m) => m.sea !== undefined || m.with !== undefined)).toBe(false);
    for (const stop of [1, 3]) {
      const g = startOpen(createGame(3, null));
      if (stop !== 3) setStopOff(g, stop, true);
      expect(g.fleets.some((f) => f.cruise !== undefined)).toBe(false);
      for (let t = 2; t <= 30; t++) {
        g.turn = t;
        navalUpkeep(g, 'USA');
        for (const f of g.fleets) if (f.nat === 'USA') { expect(atSea(f), `${f.name} ${t}`).toBe(false); expect(f.cruised).toBeUndefined(); }
      }
      // (Nor at war with Germany and at peace with everybody else, rider b593: no row names whose peace a month waits for.)
      declareWar(g, 'GER', 'USA');
      for (let t = 31; t <= 40; t++) {
        g.turn = t;
        navalUpkeep(g, 'USA');
        for (const f of g.fleets) if (f.nat === 'USA') expect(f.cruised, `${f.name} ${t}`).toBeUndefined();
      }
    }
  });
});

describe("Japan's landing craft of ruling 234(b)2 with the far side off", () => {
  it('no row of them: only the Allies have landing craft, from June 1943', () => {
    expect(NAVY.landing.craft).toEqual([[46, ['UK', 'USA'], 0.8]]);
    const g = startOpen(createGame(3, null));
    for (const t of [1, 28, 46, 70]) { g.turn = t; expect(assaultStrength(g, 'JAP'), `month ${t}`).toBe(NAVY.landing.assault); }
  });
});

describe("the book's stations, strength and dated attacks of ruling 234 (riders b568 to b583) with the far side off", () => {
  it('there is no table: nobody is stationed or cut, no attack is dated, no fleet is sent, and no corps is the book\'s', () => {
    expect(stations()).toBeNull();
    const g = startOpen(createGame(3, null));
    setStopOff(g, 1, true);
    g.turn = 28;
    const read = (): string => JSON.stringify([g.flags, g.corps.map((c) => [c.reg, c.divs, c.str, c.hold, c.stay, c.dug, c.dugFor]), g.fleets.map((f) => [f.base, f.zone, f.stance])]);
    const before = read();
    for (const nat of Object.keys(g.nations)) {
      expect(stationsBind(g, nat)).toBe(false);
      bookStations(g, nat);
      bookAttacks(g, nat);
      expect(bookFleets(g, nat)).toEqual([]);
    }
    expect(read()).toBe(before);
    expect(g.corps.some((c) => ceilinged(g, c) || c.bookWorks !== undefined || c.bookCoast !== undefined || c.bookPart !== undefined)).toBe(false);
    expect(g.fleets.some((f) => f.bookBase !== undefined)).toBe(false);
  });
});

describe('the readiness test of ruling 200(m) (rider b586 to ruling 234) with the far side off', () => {
  it('there is no test: no figures, no reading, no gathering and no flag, in any month and for any country', () => {
    const g = startOpen(createGame(3, null));
    g.turn = 28;
    g.flags['pac:freeze'] = 22;
    expect(readyRules(g)).toBeNull();
    const read = (): string => JSON.stringify([g.flags, g.fleets.map((f) => [f.base, f.zone, f.stance]), g.headlines.length]);
    const before = read();
    for (const nat of Object.keys(g.nations)) {
      expect(readiness(g, nat)).toBeNull();
      readinessMonth(g, nat);
    }
    expect(read()).toBe(before);
    expect(ops('JAP').some((op) => op.sure)).toBe(false);
  });
});

/** The garrisons the record keeps small (ruling 232's rider b606, SIEGE.startIn's rows with a size): the only corps with an establishment of their own while the far side is off. */
const SMALL: string[] = ((siege as any).startIn as any[]).filter((s) => s.str !== undefined).map((s) => s.name).sort();

describe("a corps' own establishment in a fold (rider b594 to ruling 234) with the far side off", () => {
  it('no corps has one, on the first day or by a dated row, so worn reads as it always did', () => {
    const g = startOpen(createGame(3, null));
    expect(g.corps.length).toBeGreaterThan(100);
    // (But the garrisons the record keeps small, which stand at their own size from the first day: ruling 232's rider b606.)
    expect(g.corps.filter((c) => c.full !== undefined).map((c) => c.name).sort()).toEqual(SMALL);
    const rows = (setup as any).expeditions as any[];
    expect(rows.length).toBeGreaterThan(0);
    expect(rows.filter((r) => r[r.length - 1]?.est !== undefined)).toEqual([]);
  });
});

describe('a row that lands in the lift of the corps it goes with (rider b604 to ruling 234) with the far side off', () => {
  it('no corps goes with another: every corps of a landing takes a lift of its own, and a staff takes nobody along', () => {
    expect(stations()).toBeNull();
    const g = startOpen(createGame(3, null));
    expect(g.corps.filter((c) => goesWith(g, c) !== null)).toEqual([]);
    expect(riding(g, g.corps)).toEqual([]);
    expect(g.corps.flatMap((c) => ridersOf(g, c.nat, c.name))).toEqual([]);
    // Two corps of Britain's in a home port, and a German shore: they ask for two lifts, as they always did.
    const quay = g.regions.find((r) => r.port && r.owner === 'UK' && r.ctrl === 'UK' && r.seas.length > 0 && !r.units.length)!;
    const cs = ['Test Corps A', 'Test Corps B'].map((nm) => addCorps(g, 'UK', nm, 'inf', 1, quay.id, {}));
    for (const c of cs) { c.ap = c.apMax; c.supplied = true; }
    expect(alongside(g, [cs[0]])).toEqual([]);
    const shore = g.regions.find((r) => r.ctrl === 'GER' && r.seas.length > 0 && whyNotLand(g, cs, r) === null)!;
    expect(shore).toBeDefined();
    g.flags[`lift:UK:${g.turn}`] = g.nations.UK.sealift - 1;
    expect(liftLeft(g, 'UK')).toBe(1);
    expect(whyNotLand(g, cs, shore)).toMatch(/shipping for only 1 more corps/);
    // And one corps sent alone goes alone, for its one lift.
    expect(landCorps(g, [cs[0]], shore).ok).toBe(true);
    expect(liftLeft(g, 'UK')).toBe(0);
    expect(g.regions[cs[1].reg]).toBe(quay);
  });
});

describe("the Allies' landing ships in the Pacific and the islands' garrisons (rider b614 to ruling 234) with the far side off", () => {
  it('there is no second count: every landing asks the month\'s sea lift and nothing else, in any month and on any shore', () => {
    const g = startOpen(createGame(3, null));
    for (const turn of [10, 30, 34, 52, 60]) {
      g.turn = turn;
      for (const nat of ['USA', 'UK', 'AUS', 'JAP']) for (const r of g.regions) if (r.seas.length && shoreLift(g, nat, r) !== null) throw new Error(`a count for ${nat} at ${r.name} in month ${turn}`);
    }
    const quay = g.regions.find((r) => r.port && r.owner === 'UK' && r.ctrl === 'UK' && r.seas.length > 0 && !r.units.length)!;
    const cs = ['Test Corps A', 'Test Corps B'].map((nm) => addCorps(g, 'UK', nm, 'inf', 1, quay.id, {}));
    for (const r of g.regions) expect(shipsFor(g, cs, r)).toBe(true);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('shorelift:'))).toEqual([]);
  });

  it('no row has a size by the month, and no place is a post', () => {
    const rows = (setup as any).expeditions as any[];
    expect(rows.filter((r) => Array.isArray(r[r.length - 1]?.est))).toEqual([]);
    const g = startOpen(createGame(3, null));
    expect(g.corps.filter((c) => c.full !== undefined).map((c) => c.name).sort()).toEqual(SMALL);
  });
});

describe("the plan holds the sky it lands under (rider b631 (iv) to ruling 234) with the far side off", () => {
  it("no landing plan names a shore to its air staff: a corps waiting in port for an enemy's shore, and nothing is read", () => {
    const g = startOpen(createGame(3, null));
    // Germany is at war with Poland from the first day: a German corps on a German quay, and a Polish shore.
    const quay = g.regions.find((r) => r.port && r.owner === 'GER' && r.ctrl === 'GER' && r.seas.length > 0)!;
    const shore = g.regions.find((r) => r.ctrl === 'POL' && r.seas.length > 0)!;
    expect(quay && shore && atWar(g, 'GER', 'POL')).toBe(true);
    const c = addCorps(g, 'GER', 'A corps for the far shore', 'inf', 1, quay.id, {});
    const ph = { name: 'a landing plan', from: 1, to: 6, posture: 'attack', landings: [{ corps: c.name, to: [shore.name] }] } as any;
    expect(planSkies(g, 'GER', ph, null)).toEqual([]);
  });
});

describe('home ground that answers a landing from the sea (rider b641 (f) to ruling 234) with the far side off', () => {
  it('nobody answers: a held corps stays held, whoever stands on its home ground', () => {
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'UK');
    const coast = g.regions.find((r) => r.owner === 'UK' && !r.overseas && r.port)!;
    addCorps(g, 'GER', 'Test landing corps', 'inf', 1, coast.id, {});
    addCorps(g, 'UK', 'Test home division', 'inf', 1, coast.id, { hold: 1 });
    for (const n of Object.keys(g.nations)) expect(answering(g, n), n).toEqual([]);
  });
});

describe('a main effort that leaves a city or port empty next to the enemy (the director on Canton, 16:52Z of 19 November 2026) with the far side off', () => {
  it('no place is bound and nobody is kept: every list comes back as it was given', () => {
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'POL');
    setWar(g, 'GER', 'FRA');
    for (const nat of ['GER', 'POL', 'FRA', 'UK', 'ITA', 'SOV']) {
      for (const r of g.regions) expect(cityBound(g, nat, r), `${nat} ${r.name}`).toBe(false);
      const list = g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort');
      expect(list.some((c) => g.regions[c.reg].units.length === 1 && (g.regions[c.reg].vp > 0 || g.regions[c.reg].port)), nat).toBe(true);
      expect(effortSends(g, nat, list), nat).toBe(list);
      for (const c of list) expect(effortKeeps(g, c), c.name).toBe(false);
    }
  });
});

describe('a posted division and a lost posting (rider b642 part 2 to ruling 234) with the far side off', () => {
  it('nobody is brought ashore: every row by date waits for its place as it did, whoever holds it', () => {
    const g = startOpen(createGame(3, null));
    setWar(g, 'GER', 'UK');
    setWar(g, 'ITA', 'UK');
    let read = 0;
    for (const row of (setup as any).expeditions as any[]) {
      const at = g.byName[row[5]];
      if (!at) continue;
      const was = at.ctrl;
      for (const holder of [was, 'UK', 'GER', 'SOV', 'SWI']) {
        at.ctrl = holder;
        expect(ashoreFor(g, row[1], row, at), `${row[2]} with ${holder} at ${at.name}`).toBeNull();
        read++;
      }
      at.ctrl = was;
    }
    expect(read).toBeGreaterThan(500);
  });
});

describe("a player's striking fleet gathered by the freeze (rider b641 (b) to ruling 234) with the far side off", () => {
  it('nothing is read: no fleet is moved and nothing is remembered, whoever plays and whatever the flags say', () => {
    for (const human of ['GER', 'UK', 'ITA']) {
      const g = startOpen(createGame(3, human));
      g.flags['pac:freeze'] = 1;
      g.turn = 4;
      const was = g.fleets.map((f) => [f.id, f.base, f.zone, f.stance]);
      for (const n of Object.keys(g.nations)) gatherMonth(g, n);
      expect(g.fleets.map((f) => [f.id, f.base, f.zone, f.stance])).toEqual(was);
      expect(g.flags['pac:gathered']).toBeUndefined();
    }
  });
});

describe("the far side's submarine beats with the far side off (the director's 13:45Z of 22 November 2026)", () => {
  it('has no row of them: only Italy and Germany have beats, no group has a far form, and no fleet a reach of its own', () => {
    expect(Object.keys(NAVY.beats ?? {})).toEqual(['ITA', 'GER']);
    for (const B of Object.values(NAVY.beats ?? {})) { expect(B.far).toBeUndefined(); expect(B.pools).toBeUndefined(); for (const G of B.groups) expect(G.bases).toBeUndefined(); }
    const g = startOpen(createGame(3, null));
    for (let i = 0; i < 3; i++) { g.turn++; for (const nat of Object.keys(g.nations)) navalUpkeep(g, nat); }
    for (const f of g.fleets) expect(f.reach, f.name).toBeUndefined();
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
