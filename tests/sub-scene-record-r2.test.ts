// The month's submarine scene's record (release 2; ruling 241 D1, the production lead's form section D1): written
// only while the scene's viewing setting is on, read by no rule. Off, every save is release 1's; on, the same once
// the record is stripped.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { navalUpkeep, subScene, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { SUBSCENE_ENV } from '../src/campaign/stage';
import { grantTech } from '../src/campaign/tech';
import type { Fleet, Game, Ships, SubLook } from '../src/campaign/types';

afterEach(() => { subScene.on = false; });

const twin = (g: Game): Game => loadGame(saveGame(g))!;
const stripped = (g: Game): string => { const d = JSON.parse(saveGame(g)); delete d.subLook; return JSON.stringify(d); };

describe('the submarine scene\'s record', () => {
  it('is off unless the setting is on: nothing written, the save is release 1\'s', () => {
    expect(SUBSCENE_ENV).toBe(false);
    const g = startOpen(createGame(1, null));
    navalUpkeep(g, 'UK');
    expect(g.subLook).toBeUndefined();
    expect('subLook' in JSON.parse(saveGame(g))).toBe(false);
  });

  it('on, writes the month\'s hunts and convoys, and moves nothing else', () => {
    const a = startOpen(createGame(1, null)), b = twin(a);
    navalUpkeep(a, 'UK');
    subScene.on = true;
    navalUpkeep(b, 'UK');
    subScene.on = false;
    expect(stripped(b)).toBe(saveGame(a));
    const l = b.subLook!.find((x) => x.nat === 'UK' && x.turn === b.turn)!;
    const hunt = l.hunts.find((h) => h.quarry.some((q) => q.nat === 'GER'))!;
    expect(hunt).toBeDefined();
    expect(hunt.hunters.length + (hunt.air > 0 ? 1 : 0)).toBeGreaterThan(0);
    expect(hunt.sunk).toBe(a.fleets.filter((f) => hunt.quarry.some((q) => q.fleet === f.id)).reduce((s, f) => s + f.lost.ss, 0));
    const c = l.convoys.find((x) => x.route === 'atlantic')!;
    expect(c.tons).toBe(b.convoys.atlantic.tons);
    const raiders = c.seas.flatMap((s) => s.raiders);
    expect(raiders.some((r) => r.nat === 'GER' && r.n > 0)).toBe(true);
    expect(c.seas.flatMap((s) => s.escorts).some((e) => e.nat === 'UK')).toBe(true);
    expect(Math.abs(c.seas.reduce((s, x) => s + x.tons, 0) - c.tons)).toBeLessThanOrEqual(c.seas.length * 1000);
  });

  it('names a torpedoing\'s possible attackers and picks one on its own key, without moving the torpedo\'s roll', () => {
    let seen = 0;
    for (let t = 1; t <= 60 && !seen; t++) {
      const a = startOpen(createGame(1, null));
      const home = a.fleets.find((f) => f.nat === 'UK' && f.ships.bb > 0)!;
      const boats = a.fleets.find((f) => f.nat === 'GER' && f.ships.ss > 0)!;
      home.ships.dd = 0;
      boats.zone = home.zone; boats.stance = 'raid';
      a.turn = t;
      const b = twin(a);
      const lost0 = { ...home.lost };
      navalUpkeep(a, 'UK');
      subScene.on = true;
      navalUpkeep(b, 'UK');
      subScene.on = false;
      expect(stripped(b)).toBe(saveGame(a));
      const tp = b.subLook?.flatMap((l) => l.torpedoes) ?? [];
      if (!tp.length) continue;
      seen++;
      expect(tp[0].fleet).toBe(home.id);
      expect(tp[0].could).toContain(boats.id);
      expect(tp[0].could).toContain(tp[0].by);
      // (The production lead's finding 4: the nations of the boats that could have fired and of the one picked.)
      expect(tp[0].couldNat).toEqual(tp[0].could.map((id) => b.fleets.find((f) => f.id === id)!.nat));
      expect(tp[0].byNat).toBe(b.fleets.find((f) => f.id === tp[0].by)!.nat);
      // (Ruling 240, C3, on the join: a hit damage control saves is staged as saved, not sunk.)
      expect(tp[0].sunk).toBe(b.fleets.find((f) => f.id === home.id)!.lost[tp[0].cls] > lost0[tp[0].cls]);
    }
    expect(seen).toBe(1);
  });

  it('in a history campaign, stages the raiders\' boats as the month began, before the convoy\'s own roll sinks some', () => {
    let seen = 0;
    for (let t = 2; t <= 40 && !seen; t++) {
      const a = createGame(1, null);
      a.turn = t;
      const before = new Map(a.fleets.map((f) => [f.id, f.ships.ss]));
      const b = twin(a);
      navalUpkeep(a, 'UK');
      if (!(a.convoys.atlantic?.subsLost > 0)) continue;
      seen++;
      subScene.on = true;
      navalUpkeep(b, 'UK');
      subScene.on = false;
      expect(stripped(b)).toBe(saveGame(a));
      const raiders = b.subLook!.find((l) => l.nat === 'UK')!.convoys.find((c) => c.route === 'atlantic')!.seas.flatMap((s) => s.raiders);
      expect(raiders.length).toBeGreaterThan(0);
      for (const r of raiders) expect(r.n).toBe(before.get(r.fleet));
      // The roll did sink boats: the record is not the fleets as they stand after it.
      expect(raiders.some((r) => r.n > b.fleets.find((f) => f.id === r.fleet)!.ships.ss)).toBe(true);
    }
    expect(seen).toBe(1);
  });

  it('records a raider\'s heavy ships, and moves nothing', () => {
    const a = createGame(1, null);
    a.turn = 6;
    const raider = a.fleets.find((f) => f.nat === 'GER' && f.ships.ss > 0 && f.stance === 'raid')!;
    raider.ships.ca += 1; raider.ships.bb += 1;
    const heavy = raider.ships.bb + raider.ships.cv + raider.ships.ca;
    const b = twin(a);
    navalUpkeep(a, 'UK');
    subScene.on = true;
    navalUpkeep(b, 'UK');
    subScene.on = false;
    expect(stripped(b)).toBe(saveGame(a));
    const seas = b.subLook!.find((l) => l.nat === 'UK')!.convoys.flatMap((c) => c.seas);
    const r = seas.flatMap((s) => s.raiders).find((x) => x.fleet === raider.id)!;
    expect(r).toBeDefined();
    expect(r.heavy).toBe(heavy);
    // Only a convoy's raiders carry it: the escorts and the hunts' fleets do not.
    expect(seas.flatMap((s) => s.escorts).every((e) => e.heavy === undefined)).toBe(true);
  });

  it('keeps this month and last, and goes through a save', () => {
    const g = startOpen(createGame(1, null));
    subScene.on = true;
    for (let i = 0; i < 3; i++) { navalUpkeep(g, 'UK'); g.turn++; }
    g.turn--;
    const turns = [...new Set(g.subLook!.map((l) => l.turn))].sort();
    expect(turns).toEqual([g.turn - 1, g.turn]);
    expect(twin(g).subLook).toEqual(g.subLook);
  });
});

/**
 * A grand campaign at its start with Units decide on or off, every fleet in harbour, and the United States in the
 * Allies' camp. The British have depth charges, radar, ahead-throwers and escort groups; the Americans depth charges
 * alone. German boats raid in the Irish Sea; with 'escort', twenty British destroyers escort the Atlantic convoys from
 * the Western Approaches; thirty American destroyers hunt in the Irish Sea.
 */
function open(units: boolean, escort = true): Game {
  const g = startOpen(createGame(1, null));
  if (units) g.flags.unitsDecide = 1; else delete g.flags.unitsDecide;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  g.nations.USA.align = 'allies';
  for (const id of ['sonar', 'asvradar', 'hedgehog', 'escortgroups']) grantTech(g, 'UK', id);
  grantTech(g, 'USA', 'sonar');
  sea(g, 'GER', 'irish', { ss: 20 }, 'raid');
  if (escort) sea(g, 'UK', 'wapp', { dd: 20 }, 'escort', 'atlantic');
  sea(g, 'USA', 'irish', { dd: 30 }, 'hunt');
  return g;
}
/** One of this nation's fleets, put to sea in a sea with only these ships, fully ready. */
function sea(g: Game, nat: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string): Fleet {
  const f = g.fleets.find((x) => x.nat === nat)!;
  f.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
/** The British month with the scene's setting on, and the record's Atlantic convoy row. */
function atlantic(g: Game): SubLook['convoys'][number] {
  subScene.on = true;
  try { navalUpkeep(g, 'UK'); } finally { subScene.on = false; }
  return g.subLook!.find((l) => l.nat === 'UK' && l.turn === g.turn)!.convoys.find((c) => c.route === 'atlantic')!;
}
const UK_KIT = { asdic: true, radar: true, hedgehog: true, hfdf: false, groups: true };
const USA_KIT = { asdic: true, radar: false, hedgehog: false, hfdf: false, groups: false };

describe('the submarine scene\'s record of each answering navy\'s kit (Units decide)', () => {
  it('keys each answering navy\'s own kit by navy, keeps the most destroyers\' kit as kit, and moves nothing else', () => {
    const a = open(true), b = twin(a);
    navalUpkeep(a, 'UK');
    const c = atlantic(b);
    expect(stripped(b)).toBe(saveGame(a));
    const irish = c.seas.find((s) => s.zone === zoneOf(b, 'irish'))!;
    // Each navy's entry is from its own research: the British have radar, ahead-throwers and escort groups, the Americans none.
    expect(irish.kits).toEqual({ UK: UK_KIT, USA: USA_KIT });
    // The Americans' thirty destroyers outnumber the British twenty, so the one kit is theirs, as answerer gives it.
    expect(irish.kit).toEqual(irish.kits!.USA);
    // The hunting group answers only in its own sea, and the close escort in every sea of the route.
    const rest = c.seas.filter((s) => s !== irish);
    expect(rest.length).toBe(2);
    for (const s of rest) { expect(s.kits).toEqual({ UK: UK_KIT }); expect(s.kit).toEqual(UK_KIT); }
  });

  it('gives the convoy\'s own navy\'s kit alone where no navy answers', () => {
    const g = open(true, false), c = atlantic(g);
    // Only the American group answers in the Irish Sea, so its kit stands alone there; in the other two seas none answers.
    const irish = c.seas.find((s) => s.zone === zoneOf(g, 'irish'))!;
    expect(irish.kits).toEqual({ USA: USA_KIT });
    expect(irish.kit).toEqual(USA_KIT);
    for (const s of c.seas.filter((x) => x !== irish)) { expect(s.kits).toEqual({ UK: UK_KIT }); expect(s.kit).toEqual(UK_KIT); }
  });

  it('carries escort-group training as each navy\'s research has it', () => {
    for (const groups of [false, true]) {
      const g = open(true);
      if (groups) grantTech(g, 'USA', 'escortgroups');
      const irish = atlantic(g).seas.find((s) => s.zone === zoneOf(g, 'irish'))!;
      expect(irish.kits!.USA.groups).toBe(groups);
      expect(irish.kit!.groups).toBe(groups);
      expect(irish.kits!.UK.groups).toBe(true);
    }
  });

  it('with Units decide off, writes neither kit nor kits', () => {
    const c = atlantic(open(false));
    expect(c.seas.length).toBe(3);
    for (const s of c.seas) { expect('kit' in s).toBe(false); expect('kits' in s).toBe(false); }
  });

  it('with the scene\'s setting off, writes no record at all', () => {
    const g = open(true);
    navalUpkeep(g, 'UK');
    expect(g.subLook).toBeUndefined();
    expect('subLook' in JSON.parse(saveGame(g))).toBe(false);
  });
});
