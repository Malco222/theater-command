// The air version's open pieces (b482 item 3), each behind its own switch: guns at half by night (ruling 107(b)),
// guns at the start (124), and wrecking an airfield (151(b)). With the switches off nothing changes.
import { afterEach, describe, expect, it } from 'vitest';
import { AIR, assessAir, fightersOver, gunsAt, whyNotMission as whyNotFly } from '../src/campaign/air';
import { groupRow } from '../src/campaign/airwar';
import { AIRV, airPiece, fieldWrecked, layStartGuns, nightGunShare, openAirVersion, setAirPiece, startGunLevel, startGunSites, startGunsOnLoad, wrecked } from '../src/campaign/airversion';
import { createGame, setWar } from '../src/campaign/game';
import { majorsOf } from '../src/campaign/diplomacy';
import { INTEL, MISSIONS, intelTick, setMission, spyRoom, whyNotMission } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { setSecretWar } from '../src/campaign/secretwar';
import { startOpen } from '../src/campaign/staff';
import type { Agent, AirFleet, Game, Mission } from '../src/campaign/types';

const free = (seed = 7, human: string | null = null): Game => startOpen(createGame(seed, human));
const grp = (g: Game, nat: string, name: string): AirFleet => g.airFleets.find((f) => f.nat === nat && f.name === name)!;
const id = (g: Game, n: string): number => g.byName[n].id;
const keep = structuredClone(AIRV);
const keepIntel = structuredClone(INTEL.enemy);
const keepMissions = structuredClone(INTEL.missions);
afterEach(() => { Object.assign(AIRV, structuredClone(keep)); Object.assign(INTEL.enemy, structuredClone(keepIntel)); Object.assign(INTEL.missions, structuredClone(keepMissions)); });

describe('the switches', () => {
  it('a new campaign takes the field and the start guns on and the night guns off, keeps them with the save, and touches nothing outside the grand campaign', () => {
    // (Brian, 22 November, release 2: WRECK THE AIRFIELD is on for new grand campaigns. 24 November 22:11Z: the start
    // guns are on too, as in 1.5.1; the night guns stay off.)
    const g = free();
    expect(airPiece(g, 'night')).toBe(false);
    expect(airPiece(g, 'start')).toBe(true);
    expect(airPiece(g, 'field')).toBe(true);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('av:')).sort()).toEqual(['av:field', 'av:start']);
    setAirPiece(g, 'night', true);
    setAirPiece(g, 'field', false);
    const back = loadGame(saveGame(g))!;
    expect(airPiece(back, 'night')).toBe(true);
    expect(airPiece(back, 'field')).toBe(false);
    const h = createGame(7, null);
    setAirPiece(h, 'night', true);
    setAirPiece(h, 'field', true);
    expect(airPiece(h, 'night')).toBe(false);
    expect(airPiece(h, 'field')).toBe(false);
  });

  it('a campaign begun with the data switches off takes none of them', () => {
    AIRV.nightGuns.on = AIRV.field.on = AIRV.startGuns.on = false;
    const g = free();
    for (const p of ['night', 'start', 'field'] as const) expect(airPiece(g, p)).toBe(false);
    expect(Object.keys(g.flags).some((k) => k.startsWith('av:'))).toBe(false);
  });

  it('a campaign begun with the data switches on takes them', () => {
    AIRV.nightGuns.on = AIRV.field.on = AIRV.startGuns.on = true;
    const g = free();
    for (const p of ['night', 'start', 'field'] as const) expect(airPiece(g, p)).toBe(true);
  });
});

describe('guns at half by night (ruling 107(b))', () => {
  it('by night level 3 guns bring down half as many bombers; by day the same; the bombing they stop the same', () => {
    const g = free();
    g.turn = 12;
    const bremen = id(g, 'Bremen');
    g.flags[`wk:aa:${bremen}`] = 3;
    const ids = [grp(g, 'UK', 'No. 1 Group'), grp(g, 'UK', 'No. 2 Group')];
    const before = { night: assessAir(g, ids, 'night', bremen), day: assessAir(g, ids, 'bomb', bremen) };
    expect(nightGunShare(g, true)).toBe(1);
    setAirPiece(g, 'night', true);
    expect(nightGunShare(g, true)).toBe(AIRV.nightGuns.share);
    expect(nightGunShare(g, false)).toBe(1);
    const after = { night: assessAir(g, ids, 'night', bremen), day: assessAir(g, ids, 'bomb', bremen) };
    expect(after.day.bomberLoss).toBe(before.day.bomberLoss);
    expect(after.day.gunLoss).toBe(before.day.gunLoss);
    expect(after.night.bomberLoss).toBeLessThan(before.night.bomberLoss);
    // The guns' own part is halved; the standing light flak is not.
    const flat = AIR.combat.night.flak * 0.7;
    expect(after.night.gunLoss - flat).toBeCloseTo((before.night.gunLoss - flat) * 0.5, 6);
    expect(after.night.aa).toBe(3);
  });
});

describe('guns at the start (ruling 124)', () => {
  it('every great power: its capital and its two home provinces with the most factories', () => {
    const g = free();
    for (const nat of majorsOf(g)) {
      const sites = startGunSites(g, nat);
      expect(sites.length, nat).toBe(3);
      expect(sites[0], nat).toBe(g.nations[nat].capital);
      for (const s of sites) expect(g.regions[s].owner, nat).toBe(nat);
      const facs = g.regions.filter((r) => r.owner === nat && r.ctrl === nat && r.id !== g.nations[nat].capital).map((r) => r.fac).sort((a, b) => b - a);
      expect(g.regions[sites[1]].fac, nat).toBe(facs[0]);
      expect(g.regions[sites[2]].fac, nat).toBe(facs[1]);
    }
  });

  it('level 1 in a start of 1939 or 1940, level 2 later; a province keeps the higher level', () => {
    expect(startGunLevel(1)).toBe(1);
    expect(startGunLevel(16)).toBe(1);
    expect(startGunLevel(17)).toBe(2);
    AIRV.startGuns.on = false; // (laid by hand here, so the campaign opens bare)
    const g = free();
    const berlin = g.nations.GER.capital;
    expect(gunsAt(g, berlin)).toBe(0);
    g.flags[`wk:aa:${startGunSites(g, 'UK')[1]}`] = 3;
    layStartGuns(g, majorsOf(g));
    expect(gunsAt(g, berlin)).toBe(1);
    expect(gunsAt(g, startGunSites(g, 'UK')[1])).toBe(3);
    expect(gunsAt(g, g.nations.USA.capital)).toBe(1);
  });

  it('a campaign begun with the switch on opens with them, player and computer alike; off, none', () => {
    AIRV.startGuns.on = false;
    const off = free(7, 'GER');
    expect(off.regions.some((r) => gunsAt(off, r.id) > 0)).toBe(false);
    AIRV.startGuns.on = true;
    const g = free(7, 'GER');
    for (const nat of majorsOf(g)) for (const s of startGunSites(g, nat)) expect(gunsAt(g, s), nat).toBe(1);
  });

  // (1.5.1; Brian, 24 November 2026 22:31Z: "Yes, when I load a saved war".)
  it('a grand campaign saved before they were on lays them once as it is loaded, player and computer alike', () => {
    AIRV.startGuns.on = false;
    const g = free(7, 'GER');
    g.turn = 30; // (played on into 1942: the level is the month the war began, 1939's)
    const back = loadGame(saveGame(g))!;
    AIRV.startGuns.on = true;
    expect(airPiece(back, 'start')).toBe(false);
    expect(back.regions.some((r) => gunsAt(back, r.id) > 0)).toBe(false);
    expect(startGunsOnLoad(back, majorsOf(back))).toBe(true);
    expect(airPiece(back, 'start')).toBe(true);
    for (const nat of majorsOf(back)) for (const s of startGunSites(back, nat)) expect(gunsAt(back, s), nat).toBe(1);
    // Once: loaded again, nothing more is laid; a war begun with them is never touched.
    for (const s of startGunSites(back, 'GER')) back.flags[`wk:aa:${s}`] = 0;
    expect(startGunsOnLoad(back, majorsOf(back))).toBe(false);
    expect(gunsAt(back, back.nations.GER.capital)).toBe(0);
    const now = free(7, 'GER');
    expect(startGunsOnLoad(now, majorsOf(now))).toBe(false);
  });

  it('on load: the level of the month the war began; the higher level kept; only ground each power still holds', () => {
    AIRV.startGuns.on = false;
    const g = free(7, 'UK');
    g.flags.openFrom = g.turn = 17; // (a war begun in 1941)
    const london = g.nations.UK.capital;
    const paris = g.nations.FRA.capital;
    g.regions[paris].ctrl = 'GER'; // (Paris fallen: neither France's guns nor Germany's)
    const uk = startGunSites(g, 'UK');
    g.flags[`wk:aa:${uk[1]}`] = 3;
    AIRV.startGuns.on = true;
    expect(startGunsOnLoad(g, majorsOf(g))).toBe(true);
    expect(gunsAt(g, london)).toBe(2);
    expect(gunsAt(g, uk[1])).toBe(3);
    expect(gunsAt(g, uk[2])).toBe(2);
    expect(gunsAt(g, paris)).toBe(0);
    expect(startGunSites(g, 'FRA')).not.toContain(paris);
    for (const s of startGunSites(g, 'FRA')) expect(gunsAt(g, s)).toBe(2);
  });

  it('on load: never outside the grand campaign, and never with the switch off', () => {
    const h = createGame(7, 'GER');
    expect(startGunsOnLoad(h, majorsOf(h))).toBe(false);
    expect(h.regions.some((r) => gunsAt(h, r.id) > 0)).toBe(false);
    AIRV.startGuns.on = false;
    const g = free(7, 'GER');
    expect(startGunsOnLoad(g, majorsOf(g))).toBe(false);
    expect(airPiece(g, 'start')).toBe(false);
    expect(g.regions.some((r) => gunsAt(g, r.id) > 0)).toBe(false);
  });
});

describe('wrecking an airfield (ruling 151(b))', () => {
  const plant = (g: Game, at: string, mission: Mission = 'watch', nat = 'UK'): Agent => {
    const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp: 0, reg: g.byName[at].id, ready: g.turn, mission, born: g.turn };
    g.intel.agents.push(a);
    return a;
  };
  /** Britain's player at war with Germany, a German group based at Bremen, and a British agent there. */
  function scene(on = true): { g: Game; a: Agent; f: AirFleet; bremen: number } {
    const g = free(7, 'UK');
    setWar(g, 'UK', 'GER');
    setAirPiece(g, 'field', on);
    const bremen = id(g, 'Bremen');
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.ship === undefined && x.ftr > 0)!;
    f.base = bremen;
    return { g, a: plant(g, 'Bremen'), f, bremen };
  }

  it('is offered only with its switch, and says why not', () => {
    const off = scene(false);
    expect(spyRoom(off.g)!.agents.find((x) => x.id === off.a.id)!.missions.map((m) => m.id)).not.toContain('airfield');
    expect(whyNotMission(off.g, 'UK', off.a.id, 'airfield')).toBe('Not in this campaign.');
    const { g, a } = scene();
    expect(MISSIONS).toContain('airfield');
    const offer = spyRoom(g)!.agents.find((x) => x.id === a.id)!.missions.find((m) => m.id === 'airfield')!;
    expect(offer.label).toBe('Wreck the airfield');
    expect(offer.success).toBeCloseTo(INTEL.missions.factory.success!, 9);
    expect(offer.ok).toBe(true);
    const empty = plant(g, 'Hanover');
    for (const f of g.airFleets) if (f.base === empty.reg) f.base = id(g, 'Bremen');
    expect(whyNotMission(g, 'UK', empty.id, 'airfield')).toBe('No enemy air group flies from Hanover.');
  });

  it('when it comes off, every group on the field is grounded for the month, and says so', () => {
    let hit: Game | null = null;
    for (let seed = 1; seed < 40 && !hit; seed++) {
      const s = scene();
      s.g.seed = seed;
      expect(setMission(s.g, 'UK', s.a.id, 'airfield')).toBe(true);
      intelTick(s.g, 'UK');
      if (fieldWrecked(s.g, s.bremen)) hit = s.g;
    }
    expect(hit).not.toBeNull();
    const g = hit!;
    const onField = g.airFleets.filter((f) => f.base === id(g, 'Bremen') && f.ship === undefined && f.nat === 'GER');
    for (const f of onField) {
      expect(wrecked(g, f)).toBe(true);
      expect(whyNotFly(g, f, 'bomb', id(g, 'East Anglia'))).toBe('Grounded this month: saboteurs wrecked the field at Bremen.');
      expect(groupRow(g, f, 'GER').sky).toEqual(['Grounded this month: saboteurs wrecked the field at Bremen.']);
    }
    expect(g.headlines.some((h) => /saboteurs wreck the field at Bremen/.test(h.text))).toBe(true);
    // Nothing comes up from the wrecked field; next month it flies again.
    const over = fightersOver(g, id(g, 'Bremen'), 'UK');
    g.turn++;
    expect(wrecked(g, onField[0])).toBe(false);
    expect(fightersOver(g, id(g, 'Bremen'), 'UK')).toBeGreaterThanOrEqual(over);
  });

  it("names the groups grounded in plain words: 'the group', 'both groups', 'all three'", () => {
    INTEL.missions.airfield.success = 1;
    const said: Record<number, string> = {};
    for (const n of [1, 2, 3]) {
      for (let seed = 1; seed < 40 && !said[n]; seed++) {
        const s = scene();
        s.g.seed = seed;
        for (const f of s.g.airFleets) if (f.base === s.bremen) f.base = id(s.g, 'Hanover');
        s.g.airFleets.filter((f) => f.nat === 'GER' && f.ship === undefined && f.ftr + f.bmr > 0).slice(0, n).forEach((f) => { f.base = s.bremen; });
        expect(setMission(s.g, 'UK', s.a.id, 'airfield')).toBe(true);
        intelTick(s.g, 'UK');
        const h = s.g.headlines.find((x) => x.text.startsWith(`Agent ${s.a.name}'s saboteurs wreck the field at Bremen.`));
        if (h) said[n] = h.text.slice(h.text.indexOf('Bremen.') + 8);
      }
    }
    expect(said).toEqual({
      1: 'The German air group based there is grounded this month.',
      2: 'Both German air groups based there are grounded this month.',
      3: 'All 3 German air groups based there are grounded this month.',
    });
  });

  it("the other side's services can wreck the player's field; off, the plots go as before", () => {
    INTEL.enemy.chance = 1;
    INTEL.enemy.quiet = 0;
    AIRV.field.enemyShare = 1;
    // (The plot that strikes at once: spy hunters off, as this test means. With them on, tests/secretwar.test.ts.)
    const g = free(7, 'UK');
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = 0;
    setSecretWar(g, 'hunt', false);
    setAirPiece(g, 'field', true);
    intelTick(g, 'UK');
    const line = g.headlines.find((h) => /^Grounded this month: saboteurs wrecked the field at /.test(h.text));
    expect(line?.text).toMatch(/Your police blame German agents\.$/);
    const h = free(7, 'UK');
    setWar(h, 'UK', 'GER');
    h.flags.spyfrom = 0;
    setSecretWar(h, 'hunt', false);
    setAirPiece(h, 'field', false);
    intelTick(h, 'UK');
    expect(h.headlines.some((x) => /field/.test(x.text))).toBe(false);
  });
});
