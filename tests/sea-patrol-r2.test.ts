// The sea patrol against submarines (release 2, Units decide; the design director's ruling of 24 November 2026 15:14Z
// on naval-battles/convoy-r2/sea-patrol-form.md, (a) on all nine questions, with his marks). Any land bomber group can
// patrol one sea in its reach; its aircraft watch s = R(1 - e^(-A/A_R)) of it, A_R = a0 R / air. With anti-submarine
// kit they attack and sink boats on station there and cut the raid by OEG T7 at the share watched; without it they keep
// the boats down. Every figure is unread as shipped, and then nothing of it is read. The figures in these tests are the
// tests' own, not the game's.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { allied } from '../src/campaign/game';
import { CAUSE, NAVY, airKit, aswKit, convoyCount, huntersIn, navalUpkeep, onStation, onWatch, quieterBeat, seaCover, seaRoutes, seaSquares, zoneOf } from '../src/campaign/navy';
import { AIR, airRange, airfieldsHold, assessAir, fieldClass, setMission, whyNotMission } from '../src/campaign/air';
import { groupJobs, groupRow } from '../src/campaign/airwar';
import { patrolAircraft, patrolOn, patrolReach, patrolSeas, patrolStaff, watchAgainst, watchFor } from '../src/campaign/seapatrol';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { skyAtSea } from '../src/campaign/weather';
import type { AirFleet, Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const UNREAD = structuredClone(CAUSE.patrol);
const FIG = { reach: 'map' as 'map' | 'air', a0: 50, intensity: 0.5, attacks: { kit: 0.6, none: 0.3 }, kill: { kit: 0.1, none: 0.04 }, lost: null as number | null };
const lay = (fig: typeof FIG = FIG): void => { Object.assign(CAUSE.patrol, structuredClone(fig)); };

function open(seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  g.flags.unitsDecide = 1;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
function sea(g: Game, name: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string): Fleet {
  const f = g.fleets.find((x) => x.name === name)!;
  f.ships = { ...none(), ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
const group = (g: Game, name: string): AirFleet => g.airFleets.find((f) => f.name === name)!;
const ukSide = (g: Game) => (n: string): boolean => allied(g, n, 'UK');

afterEach(() => { Object.assign(CAUSE.patrol, structuredClone(UNREAD)); });

describe('as shipped: every figure unread', () => {
  it('lays none of the figures', () => {
    expect(CAUSE.patrol).toEqual({ reach: 'map', a0: null, intensity: null, attacks: { kit: null, none: null }, kill: { kit: null, none: null }, lost: null });
  });

  it('offers no patrol, reads none of it, and keeps the flat air term and the coasts\' cover (never remove a working cause before its replacement is laid)', () => {
    const g = open();
    grantTech(g, 'UK', 'asvradar');
    const grp = group(g, 'No. 1 Group'), wapp = zoneOf(g, 'wapp'), irish = zoneOf(g, 'irish');
    expect(patrolOn(g)).toBe(false);
    expect(whyNotMission(g, grp, 'seapatrol', wapp)).not.toBeNull();
    expect(setMission(g, grp, 'seapatrol', wapp)).toBe(false);
    expect(patrolSeas(g, grp)).toEqual([]);
    expect(groupJobs(g, grp.id, 'UK').some((j) => j.key.startsWith('sea:'))).toBe(false);
    expect(seaCover(g, 'UK', wapp).kind).toBe('shore');
    expect(huntersIn(g, 'UK', irish).weight).toBeGreaterThan(0);
    // (Once laid, the coasts' cover and the flat term give way to the aircraft on patrol.)
    lay();
    expect(patrolOn(g)).toBe(true);
    expect(seaCover(g, 'UK', wapp).kind).toBe('none');
    expect(huntersIn(g, 'UK', irish).weight).toBe(0);
  });

  it('is not there at all without Units decide, figures laid or not', () => {
    lay();
    const g = open();
    delete g.flags.unitsDecide;
    expect(patrolOn(g)).toBe(false);
    expect(whyNotMission(g, group(g, 'No. 1 Group'), 'seapatrol', zoneOf(g, 'wapp'))).not.toBeNull();
  });
});

describe('who flies it and where (Q1, Q2)', () => {
  beforeEach(() => lay());

  it('a land bomber group can patrol a sea in its reach; fighters and carrier wings cannot', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    expect(whyNotMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp)).toBeNull();
    expect(whyNotMission(g, group(g, 'No. 11 Group'), 'seapatrol', wapp)).toMatch(/fighters/i);
    expect(whyNotMission(g, group(g, 'Home Fleet strike wing'), 'seapatrol', wapp)).not.toBeNull();
    // (A German division in Germany does not reach the Western Approaches: the strike's own reach.)
    expect(whyNotMission(g, group(g, '1. Fliegerdivision'), 'seapatrol', wapp)).toMatch(/beyond/i);
  });

  it('a bomber group on a field strip flies no patrol from it, as it flies no raid (the air thread\'s Q2)', () => {
    const g = open(), wapp = zoneOf(g, 'wapp'), grp = group(g, 'No. 1 Group');
    g.flags.fields = 1; g.flags.airwar = 1;
    const strip = g.regions.find((r) => r.ctrl === 'UK' && !r.overseas && fieldClass(g, r.id) === 1 && airfieldsHold(g, r.id) > 0)!;
    expect(strip).toBeDefined();
    grp.base = strip.id;
    expect(grp.kind).toBe('bmr');
    expect(whyNotMission(g, grp, 'seapatrol', wapp)).toMatch(/concrete airfield/);
  });

  it('reaches the share of the sea\'s squares on the chart within its range of its airfield, or as first ruled the sea\'s air share; all of it with long-range patrol aircraft', () => {
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl'), grp = group(g, 'No. 1 Group');
    expect(CAUSE.patrol.reach).toBe('map');
    const sq = seaSquares(g);
    const share = (z: number): number => {
      const at = sq.at.get(z)!, r = g.regions[grp.base], lim = airRange(grp, g).bmr / AIR.kmPerUnit;
      return at.filter((p) => Math.hypot(p.x - r.x, p.y - r.y) <= lim).length / at.length;
    };
    const w = patrolReach(g, grp, wapp)!, n = patrolReach(g, grp, natl)!;
    expect(w.R).toBeCloseTo(share(wapp), 12);
    expect(n.R).toBeCloseTo(share(natl), 12);
    expect(n.R).toBeGreaterThan(0);
    expect(n.R).toBeLessThan(1);
    expect(w.nm2).toBeCloseTo(w.R * sq.at.get(wapp)!.length * sq.nm2, 6);
    CAUSE.patrol.reach = 'air';
    expect(patrolReach(g, grp, wapp)!.R).toBe(g.seas[wapp].air);
    CAUSE.patrol.reach = 'map';
    grantTech(g, 'UK', 'patrolaircraft');
    expect(patrolReach(g, grp, wapp)!.R).toBe(1);
    expect(patrolReach(g, grp, natl)!.R).toBe(1);
  });

  it('a sea with no coast only with long-range patrol aircraft, from within range of a coast of a sea next to it', () => {
    const g = open(), grp = group(g, 'No. 1 Group');
    const gap = g.seas.find((z) => z.name === 'Mid-Atlantic Gap')!;
    expect(gap.coast.length).toBe(0);
    expect(patrolReach(g, grp, gap.id)).toBeNull();
    grantTech(g, 'UK', 'patrolaircraft');
    const at = patrolReach(g, grp, gap.id)!;
    expect(at.R).toBe(1);
    expect(at.d).toBeCloseTo(Math.min(...gap.adj.filter((a) => g.seas[a].coast.length).map((a) => patrolReach(g, grp, a)?.d ?? Infinity)), 9);
  });

  it('is offered over the seas of its side\'s convoy routes in its reach, as a standing order that takes the group from every other mission', () => {
    const g = open(), grp = group(g, 'No. 1 Group'), wapp = zoneOf(g, 'wapp');
    const seas = patrolSeas(g, grp);
    expect(seas).toContain(wapp);
    for (const z of seas) expect(seaRoutes(g).some((r) => r.zones.includes(z) && allied(g, r.to, 'UK'))).toBe(true);
    // (Germany's North Sea route is not the British group's to watch.)
    expect(seas).not.toContain(zoneOf(g, 'nsea'));
    expect(groupJobs(g, grp.id, 'UK').some((j) => j.key === `sea:${wapp}`)).toBe(true);
    expect(setMission(g, grp, 'seapatrol', wapp)).toBe(true);
    expect(grp.mission).toBe('seapatrol');
    expect(grp.target).toBe(wapp);
    expect(groupRow(g, grp, 'UK').doing).toMatch(/^Patrol the Western Approaches against submarines: watches about \d+% of the waters it reaches$/);
  });
});

describe('the share watched (Q3)', () => {
  beforeEach(() => lay());

  it('is R(1 - e^(-A/A_R)), A_R = a0 x the area reached / 10,000 square nautical miles (or a0 R / air as first ruled), A the group\'s bombers x readiness at full cover inside 60% of its range', () => {
    const g = open(), grp = group(g, 'No. 1 Group'), wapp = zoneOf(g, 'wapp');
    setMission(g, grp, 'seapatrol', wapp);
    const A = patrolAircraft(g, grp, wapp);
    expect(A).toBeCloseTo(grp.bmr * grp.ready, 9);
    const at = patrolReach(g, grp, wapp)!, AR = (FIG.a0 * at.nm2) / 1e4;
    const w = watchFor(g, wapp, ukSide(g))!;
    expect(w.s).toBeCloseTo(at.R * (1 - Math.exp(-A / AR)), 12);
    expect(w.A).toBeCloseTo(A, 12);
    // (As first ruled: A_R = a0 R / air.)
    CAUSE.patrol.reach = 'air';
    const R = g.seas[wapp].air;
    expect(watchFor(g, wapp, ukSide(g))!.s).toBeCloseTo(R * (1 - Math.exp(-A / ((FIG.a0 * R) / g.seas[wapp].air))), 12);
    grp.ready = 0.5;
    expect(patrolAircraft(g, grp, wapp)).toBeCloseTo(A / 2, 9);
  });

  it('rises with the aircraft on patrol and never past the waters reached', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    const one = watchFor(g, wapp, ukSide(g))!.s;
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    const two = watchFor(g, wapp, ukSide(g))!.s;
    expect(two).toBeGreaterThan(one);
    expect(two).toBeLessThan(patrolReach(g, group(g, 'No. 1 Group'), wapp)!.R);
  });

  it('none under a closed sky, half under cloud', () => {
    const g = open(), grp = group(g, 'No. 1 Group'), wapp = zoneOf(g, 'wapp');
    setMission(g, grp, 'seapatrol', wapp);
    g.flags.weather = 1;
    const seen = new Map<string, number>();
    for (let t = 1; t <= 120 && seen.size < 3; t++) { g.turn = t; seen.set(skyAtSea(g, wapp), patrolAircraft(g, grp, wapp)); }
    if (seen.has('closed')) expect(seen.get('closed')).toBe(0);
    if (seen.has('cloud') && seen.has('clear') && seen.get('clear')! > 0) expect(seen.get('cloud')! / seen.get('clear')!).toBeCloseTo(0.5, 9);
    expect(seen.size).toBeGreaterThan(1);
  });
});

describe('what the watch does to the boats (Q4, Q5)', () => {
  beforeEach(() => lay());

  it('without kit it keeps them down: their time on station there counts at 1 - s, in the convoys\' count too', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    expect(airKit(aswKit(g, 'UK'))).toBe(false);
    sea(g, 'Home Fleet', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    const boats = sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'raid');
    const before = convoyCount(g, atlantic)!.boats;
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    const w = watchAgainst(g, wapp, 'GER')!;
    expect(w.none).toBeCloseTo(w.s, 12);
    expect(w.kit).toBe(0);
    expect(onWatch(g, boats, wapp)).toBeCloseTo(onStation(g, boats, wapp) * (1 - w.s), 12);
    expect(convoyCount(g, atlantic)!.boats).toBeCloseTo(before * (1 - w.s), 9);
    // (Aircraft without kit cut no raid: no cover where they fly.)
    expect(seaCover(g, 'UK', wapp).kind).toBe('none');
  });

  it('with kit the boats stay up, and OEG T7\'s cut stands at the share watched', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    expect(airKit(aswKit(g, 'UK'))).toBe(true);
    const boats = sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'raid');
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    const w = watchAgainst(g, wapp, 'GER')!;
    expect(w.kit).toBeCloseTo(w.s, 12);
    expect(onWatch(g, boats, wapp)).toBeCloseTo(onStation(g, boats, wapp), 12);
    expect(seaCover(g, 'UK', wapp)).toEqual({ kind: 'patrol', share: w.s });
    expect(w.attacks).toBe(FIG.attacks.kit);
    expect(w.kills).toBeCloseTo(FIG.attacks.kit * FIG.kill.kit, 12);
  });

  it('a carrier with the escort still covers its convoy as before', () => {
    const g = open(), natl = zoneOf(g, 'natl');
    grantTech(g, 'UK', 'asvradar');
    sea(g, 'Home Fleet', 'natl', { dd: 10, cv: 1 }, 'escort', 'atlantic');
    expect(seaCover(g, 'UK', natl, 'atlantic').kind).toBe('carrier');
  });

  it('aircraft sink boats on station, never past the hunt\'s cap, on their own dice, and the month says how many', () => {
    lay({ ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } });
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    const boats = sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'raid');
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    navalUpkeep(g, 'UK');
    const rec = g.patrolSunk!.UK;
    expect(rec.turn).toBe(g.turn);
    const n = rec.seas[wapp] ?? 0;
    expect(n).toBeGreaterThan(0);
    expect(rec.sunk.GER).toBe(n);
    expect(40 - boats.ships.ss).toBeGreaterThanOrEqual(n);
    expect(40 - boats.ships.ss).toBeLessThanOrEqual(Math.ceil(40 * 0.25));
    expect(g.convoys.atlantic.byAircraft).toBe(n);
    expect(g.headlines.some((x) => /aircraft on patrol sink/.test(x.text))).toBe(true);
  });

  it('the same month, the same dice: two runs agree, and a run with no patrol sinks none from the air', () => {
    const run = (patrol: boolean): number => {
      lay({ ...FIG, attacks: { kit: 3, none: 3 }, kill: { kit: 0.5, none: 0.5 } });
      const g = open(3), wapp = zoneOf(g, 'wapp');
      grantTech(g, 'UK', 'asvradar');
      const b = sea(g, 'U-boat Fleet', 'wapp', { ss: 30 }, 'raid');
      if (patrol) setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
      navalUpkeep(g, 'UK');
      return b.ships.ss + 100 * (g.patrolSunk?.UK?.seas[wapp] ?? 0);
    };
    expect(run(true)).toBe(run(true));
    expect(Math.floor(run(false) / 100)).toBe(0);
  });

  it('enemy fighters meet a patrol as they meet a strike over the sea, at the patrol\'s own intensity row (the air thread\'s Q6)', () => {
    const g = open(), grp = group(g, 'No. 1 Group'), nsea = zoneOf(g, 'nsea');
    expect(whyNotMission(g, grp, 'seapatrol', nsea)).toBeNull();
    for (const f of g.airFleets) if (f.nat === 'GER' && f.kind === 'ftr') { f.mission = 'support'; f.ready = 1; }
    const at = (i: number): number => { lay({ ...FIG, intensity: i }); const a = assessAir(g, [grp], 'seapatrol', nsea); return a.def.length ? a.B * a.bomberLoss : -1; };
    const lo = at(0.2), hi = at(1);
    expect(lo).toBeGreaterThanOrEqual(0);
    expect(hi).toBeGreaterThan(lo);
  });

  it('aircraft are lost on patrol at the operational share where it is laid', () => {
    lay({ ...FIG, lost: 0.2 });
    const g = open(), wapp = zoneOf(g, 'wapp'), grp = group(g, 'No. 1 Group');
    sea(g, 'U-boat Fleet', 'wapp', { ss: 10 }, 'raid');
    setMission(g, grp, 'seapatrol', wapp);
    const was = grp.bmr;
    navalUpkeep(g, 'UK');
    expect(grp.bmr).toBeLessThan(was);
  });
});

describe('the computer\'s staffs (Q7, Q5)', () => {
  beforeEach(() => lay({ ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } }));

  it('send no group where no enemy boat is on station, and one where many are', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    const mine = g.airFleets.filter((f) => f.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol')).toBe(false);
    sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol' && f.target === wapp)).toBe(true);
    expect(mine.filter((f) => f.mission === 'seapatrol').every((f) => f.kind === 'bmr' && f.ship === undefined)).toBe(true);
  });

  it('send only groups with no fighters: a patrol flies alone, and nothing that guards the ground is taken (the air thread\'s Q7)', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    const mine = g.airFleets.filter((f) => f.nat === 'UK');
    for (const f of mine) if (f.kind !== 'ftr' && f.bmr > 0) f.ftr = 5;
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol' && f.target === wapp)).toBe(false);
  });

  it('a computer-run raider moves out of waters watched by aircraft without kit, to a sea of the route where its boats stay up longer', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp');
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    expect(quieterBeat(g, f)).toBe(-1);
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    const to = quieterBeat(g, f);
    expect(to).toBeGreaterThanOrEqual(0);
    expect(atlantic.zones.map((k) => zoneOf(g, k))).toContain(to);
    expect(onWatch(g, f, to)).toBeGreaterThan(onWatch(g, f, wapp));
  });
});

describe('the save', () => {
  it('keeps the order and the month\'s record', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp'), grp = group(g, 'No. 1 Group');
    setMission(g, grp, 'seapatrol', wapp);
    g.patrolSunk = { UK: { turn: g.turn, sunk: { GER: 2 }, seas: { [wapp]: 2 } } };
    const h = loadGame(saveGame(g))!;
    const back = h.airFleets.find((f) => f.id === grp.id)!;
    expect(back.mission).toBe('seapatrol');
    expect(back.target).toBe(wapp);
    expect(h.patrolSunk).toEqual(g.patrolSunk);
  });
});

describe('the staff follows the boats\' best answer, not last month\'s boats', () => {
  beforeEach(() => lay({ ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } }));

  it('without kit it follows the boats\' best answer: its patrols spread over the route, and the boats\' best sea there is worse for them', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const zs = atlantic.zones.map((k) => zoneOf(g, k));
    const best = (): number => Math.max(...zs.map((z) => onWatch(g, f, z)));
    const before = best();
    const mine = g.airFleets.filter((x) => x.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    const sent = mine.filter((x) => x.mission === 'seapatrol');
    // (The first group goes where the boats are; the boats would then move, and the next goes after them.)
    expect(sent.map((x) => x.target)[0]).toBe(wapp);
    expect(new Set(sent.map((x) => x.target)).size).toBe(sent.length);
    expect(sent.length).toBeGreaterThan(1);
    expect(best()).toBeLessThan(before);
  });
});
