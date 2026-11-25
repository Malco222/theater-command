// The sea patrol against submarines (release 2, Units decide; the design director's ruling of 24 November 2026 15:14Z
// on naval-battles/convoy-r2/sea-patrol-form.md, (a) on all nine questions, with his marks, and his rulings of 17:49Z).
// Any land bomber group can patrol one sea in its reach; its aircraft watch s = R(1 - e^(-A/A_R)) of it, R the share of
// the sea's squares on the chart within its bomber range ('map', which the design director ruled for the dormant interim
// at 17:49Z, to be replaced at the one a0 refit by the aircraft type's radius on the map geometry) and A_R = a0 x the
// area reached / 10,000 square nautical miles. With anti-submarine kit they attack and sink boats on station there and
// cut the raid by OEG T7 at the share watched; without it they keep the boats down. As shipped the interim figures are
// laid but the switch 'live' is false (17:49Z, 5 (a)), and then nothing of it is read. The figures in these tests are
// the tests' own, not the game's, and each lays the switch live.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { allied } from '../src/campaign/game';
import { CAUSE, FLEETS, NAVY, airKit, aswKit, atSea, convoyCount, huntersIn, laneLoss, navalAI, navalUpkeep, nearerPort, onStation, onWatch, passageSeas, quieterBeat, seaCover, seaLatitude, seaRoutes, seaSquares, subScene, zoneOf } from '../src/campaign/navy';
import { AIR, AIRWAR, airAI, airPool, airRange, airfieldsHold, assessAir, fieldClass, forecastAir, isOffensive, kmToSea, setMission, whyNotMission } from '../src/campaign/air';
import { groupJobs, groupRow } from '../src/campaign/airwar';
import { byHand } from '../src/campaign/depth';
import { patrolAircraft, patrolFigures, patrolLoss, patrolMet, patrolOn, patrolReach, patrolSeas, patrolStaff, patrolWorth, watchAgainst, watchFor } from '../src/campaign/seapatrol';
import { noteSeaSight, seenBoats } from '../src/campaign/seasight';
import causeData from '../data/campaign/convoycause.json';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { runTurnAI } from '../src/campaign/turn';
import { setUnitsDecide } from '../src/campaign/units';
import { dayShare, skyAtSea } from '../src/campaign/weather';
import { mapDist } from '../src/campaign/wrap';
import { turnToDate } from '../src/core/ticket';
import type { AirFleet, Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const UNREAD = structuredClone(CAUSE.patrol);
const FIG = { live: true, reach: 'map' as 'map' | 'air', a0: 50, intensity: 0.5 as number | null, attacks: { kit: 0.6, none: 0.3 }, kill: { kit: 0.1, none: 0.04 }, lost: null as number | null };
const lay = (fig: typeof FIG = FIG): void => { Object.assign(CAUSE.patrol, structuredClone(fig)); };

function open(seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  g.flags.unitsDecide = 1;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
function sea(g: Game, name: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string, nat?: string): Fleet {
  const f = g.fleets.find((x) => x.name === name && (!nat || x.nat === nat))!;
  f.ships = { ...none(), ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
const group = (g: Game, name: string): AirFleet => g.airFleets.find((f) => f.name === name)!;
const ukSide = (g: Game) => (n: string): boolean => allied(g, n, 'UK');
/** Every German air group stood down: no enemy fighter comes up anywhere. */
const quiet = (g: Game): void => { for (const f of g.airFleets) if (f.nat === 'GER') f.mission = 'rest'; };
/** Moves an air group to the French province nearest a sea (as the German air force came there after June 1940). */
function toFrance(g: Game, f: AirFleet, z: number): void {
  let best = Infinity, at = f.base;
  for (const r of g.regions) {
    if (r.ctrl !== 'FRA' || r.overseas) continue;
    f.base = r.id;
    const d = kmToSea(g, f, z);
    if (d < best) { best = d; at = r.id; }
  }
  f.base = at;
}
/** Every German fighter group left to defend, at full readiness: over the North Sea in 1939 they meet a patrol. */
const foe = (g: Game): void => { for (const f of g.airFleets) if (f.nat === 'GER' && f.kind === 'ftr') { f.mission = 'support'; f.target = -1; f.ready = 1; } };
/** A strong patrol, so that a month's dice sink boats. */
const STRONG = { ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } };

// Each staff reads only what its side has seen (seasight.ts; the design director's 24 November 2026 17:49Z amendment to
// ruling 1, and his ruling of 17:54Z). These give the staffs here their sight, through the record's own writer where a
// month's events can say it, so that a test of what a staff does with what it knows sets out what it knows.
/** A side's convoys met every enemy boat on station where it is this month (each fleet's boats x its share on station): it then sees the boats as they truly stand. */
function sawAll(g: Game, nat: string): void {
  const attack: Record<number, number> = {};
  for (const f of g.fleets) if (atSea(f) && f.ships.ss > 0 && g.nations[f.nat].atWar.includes(nat)) attack[f.zone] = (attack[f.zone] ?? 0) + f.ships.ss * onStation(g, f, f.zone);
  noteSeaSight(g, nat, { attack, incident: {}, sunk: {} });
}
/**
 * The boats' side has met the aircraft over every sea of the convoy routes as they now stand, as if its boats had been on
 * station in each this month: its staff then weighs the patrols truly flown. For the tests of what a move loses, which
 * are about the arithmetic of the move and not about what the boats have seen.
 */
function metEvery(g: Game, nat = 'GER'): void {
  const book = ((g.intel.seaSight ??= {})[nat] ??= {});
  for (const r of NAVY.convoys) for (const k of r.zones) {
    const z = zoneOf(g, k), w = watchAgainst(g, z, nat);
    (book[z] ??= {}).air = { t: g.turn, kit: w?.kit ?? 0, none: w?.none ?? 0, lost: 0, how: w && w.none > 0 ? ['keptDown'] : [] };
  }
}
/**
 * The boats' side has met the watch now kept over a fleet's beat, as the patrolling side's month would fly it over them
 * there (the record's writer holds it only once that month is booked, navy.ts patrolSubs; laid here without that month's
 * dice, for the tests of the move's arithmetic).
 */
function metBeat(g: Game, f: Fleet): void {
  const w = watchAgainst(g, f.station, f.nat);
  (((g.intel.seaSight ??= {})[f.nat] ??= {})[f.station] ??= {}).air = { t: g.turn, kit: w?.kit ?? 0, none: w?.none ?? 0, lost: 0, how: w && w.none > 0 ? ['keptDown'] : [] };
}

const SHARE = AIRWAR.ai.raids.share;
afterEach(() => { Object.assign(CAUSE.patrol, structuredClone(UNREAD)); AIRWAR.ai.raids.share = SHARE; });

describe('as shipped: the interim figures laid, the switch off, so the patrol is dormant', () => {
  it('lays the design director\'s interim figures, leaves intensity unread, and keeps the switch off', () => {
    // (a0 8 read through the game; attacks from the record, 8 a month over about 5 boats on station x 0.60 inside reach,
    // radar 1.1 times by day; kill 0.025 for every air force; 0.04 lost a month to weather and accident. Dormant until
    // the one a0 refit: the design director's ruling of 17:49Z, 5 (a).)
    expect(CAUSE.patrol).toEqual({ live: false, reach: 'map', a0: 8, intensity: null, attacks: { kit: 2.92, none: 2.65 }, kill: { kit: 0.025, none: 0.025 }, lost: 0.04 });
  });

  it('reads the waters reached on the map for the interim, as the design director ruled at 17:49Z, and its doc says so', () => {
    expect(CAUSE.patrol.reach).toBe('map');
    // (The data's own words: 'map' is the ruled interim reading, to be replaced at the one a0 refit by the aircraft
    // type's radius on the map geometry; 'air' was first ruled and is no longer the reading.)
    const doc = (causeData as unknown as { _patrol: string })._patrol;
    expect(doc).toMatch(/'map', as the design director ruled for the dormant interim \(his ruling of 24 November 2026 17:49Z, 4\)/);
    expect(doc).toMatch(/replaced at the one a0 refit by the aircraft type's radius on the map geometry/);
    expect(doc).not.toMatch(/'air' \(as ruled\)/);
    expect(doc).not.toMatch(/builder's amendment/);
  });

  it('the switch: off, the patrol is dormant whatever else is laid, the air fighting\'s row too', () => {
    lay({ ...FIG, live: false });
    const g = open(), wapp = zoneOf(g, 'wapp'), grp = group(g, 'No. 1 Group'), irish = zoneOf(g, 'irish');
    expect(CAUSE.patrol.intensity).not.toBeNull();
    expect(patrolFigures()).toBeNull();
    expect(patrolOn(g)).toBe(false);
    expect(whyNotMission(g, grp, 'seapatrol', wapp)).not.toBeNull();
    expect(setMission(g, grp, 'seapatrol', wapp)).toBe(false);
    expect(patrolSeas(g, grp)).toEqual([]);
    // (A group left on the order by hand watches nothing, the boats are not moved, and the staff sends no group.)
    grp.mission = 'seapatrol'; grp.target = wapp;
    expect(watchAgainst(g, wapp, 'GER')).toBeNull();
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    expect(quieterBeat(g, f)).toBe(-1);
    const mine = g.airFleets.filter((x) => x.nat === 'UK' && x !== grp);
    expect(patrolStaff(g, 'UK', mine)).toBe(false);
    expect(mine.some((x) => x.mission === 'seapatrol')).toBe(false);
    // (The working causes stand: the flat air term and the coasts' cover.)
    grantTech(g, 'UK', 'asvradar');
    expect(seaCover(g, 'UK', wapp).kind).toBe('shore');
    expect(huntersIn(g, 'UK', irish).weight).toBeGreaterThan(0);
    // (Live, the same figures run it.)
    CAUSE.patrol.live = true;
    expect(patrolOn(g)).toBe(true);
    expect(watchAgainst(g, wapp, 'GER')).not.toBeNull();
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

  it('the whole game: months of two computer wars with the shipped figures are, save for save, the same months with every figure of the patrol unread', () => {
    // (The form's claim, as convoy-cause-r2.test.ts checks its own: dormant, the patrol reads none of its laid figures
    // anywhere, so the game is the game without them to the byte. The 60-month check on two seeds is run outside the suite.)
    const months = 12;
    const play = (): string => {
      const g = startOpen(createGame(1, null));
      expect(setUnitsDecide(g, true)).toBe(true);
      for (let t = 0; t < months; t++) runTurnAI(g);
      return saveGame(g);
    };
    expect(patrolFigures()).toBeNull();
    const shipped = play();
    Object.assign(CAUSE.patrol, { reach: 'map', a0: null, intensity: null, attacks: { kit: null, none: null }, kill: { kit: null, none: null }, lost: null });
    const unread = play();
    // (Compared whole, without printing two saves on a failure.)
    expect(unread.length).toBe(shipped.length);
    expect(unread === shipped).toBe(true);
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

  it('reaches the share of the sea\'s squares on the chart within its range of its airfield (the interim reading ruled at 17:49Z), or, set to the reading first ruled, the sea\'s air share; all of it with long-range patrol aircraft', () => {
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

  it('a group that reaches none of a sea\'s waters, though its nearest coast is in range, does not reach the sea: refused, and adds nothing to a watch there', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    // (3. Fliegerdivision in Westphalia: the nearest coast of the Western Approaches inside its bombers' 900 km, none of the sea's squares.)
    const far = group(g, '3. Fliegerdivision');
    const range = airRange(far, g).bmr, r = g.regions[far.base];
    expect(kmToSea(g, far, wapp)).toBeLessThanOrEqual(range);
    expect(seaSquares(g).at.get(wapp)!.filter((p) => mapDist(g, p.x, p.y, r.x, r.y) <= range / AIR.kmPerUnit).length).toBe(0);
    expect(patrolReach(g, far, wapp)).toBeNull();
    expect(whyNotMission(g, far, 'seapatrol', wapp)).toBe('That sea is beyond its bombers.');
    expect(setMission(g, far, 'seapatrol', wapp)).toBe(false);
    // (On the order all the same, as a save from before might hold it, its aircraft add nothing to the watch of a group
    // that does reach the sea: here 4. Fliegerdivision moved to France by it.)
    const near = group(g, '4. Fliegerdivision');
    toFrance(g, near, wapp);
    expect(patrolReach(g, near, wapp)!.R).toBeGreaterThan(0);
    near.mission = 'seapatrol'; near.target = wapp;
    const alone = watchAgainst(g, wapp, 'UK')!;
    expect(alone.s).toBeGreaterThan(0);
    far.mission = 'seapatrol'; far.target = wapp;
    expect(watchAgainst(g, wapp, 'UK')).toEqual(alone);
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
    // (By day only: the boats are kept down for the day's share of their time.)
    expect(w.day).toBeCloseTo(dayShare(seaLatitude(g, wapp), turnToDate(g.turn).month), 12);
    expect(w.none).toBeCloseTo(w.s * w.day, 12);
    expect(w.kit).toBe(0);
    expect(onWatch(g, boats, wapp)).toBeCloseTo(onStation(g, boats, wapp) * (1 - w.s * w.day), 12);
    expect(convoyCount(g, atlantic)!.boats).toBeCloseTo(before * (1 - w.s * w.day), 9);
    // (Aircraft without kit cut no raid: no cover where they fly.)
    expect(seaCover(g, 'UK', wapp).kind).toBe('none');
  });

  it('without kit it keeps them down on the shipping lanes too: the boats raid a watched sea\'s lane only for their time up, as on a convoy route', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    expect(airKit(aswKit(g, 'UK'))).toBe(false);
    sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'raid');
    const before = laneLoss(g, 'UK', [wapp]);
    expect(before.raid).toBeGreaterThan(0);
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    const w = watchAgainst(g, wapp, 'GER')!;
    expect(w.none).toBeGreaterThan(0);
    const after = laneLoss(g, 'UK', [wapp]);
    // (Boats alone raid here, so the whole raid is theirs, at 1 - the share of their time the aircraft keep them down.)
    expect(after.raid).toBeCloseTo(before.raid * (1 - w.none), 9);
    expect(after.loss).toBeLessThan(before.loss);
  });

  it('with kit the boats stay up, and OEG T7\'s cut stands at the share watched', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    expect(airKit(aswKit(g, 'UK'))).toBe(true);
    const boats = sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'raid');
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    const w = watchAgainst(g, wapp, 'GER')!;
    expect(w.kit).toBeCloseTo(w.s * w.day, 12);
    expect(onWatch(g, boats, wapp)).toBeCloseTo(onStation(g, boats, wapp), 12);
    expect(seaCover(g, 'UK', wapp)).toEqual({ kind: 'patrol', share: w.s * w.day });
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

  it('on their own dice and inside the hunt\'s cap: with destroyers hunting there too, the ships sink as many with the patrol as without it, and the two together never more than the cap', () => {
    // (A patrol far stronger than the cap allows: ten times the strong figures' attacks.)
    const month = (seed: number, patrol: boolean): { hunt: number; air: number; lost: number; expected: number } => {
      lay({ ...STRONG, attacks: { kit: 60, none: 60 } });
      const g = open(seed), wapp = zoneOf(g, 'wapp');
      grantTech(g, 'UK', 'asvradar');
      // (Boats on patrol, not raiding a route of Britain's, so that the ships' hunt takes its whole share of them.)
      const b = sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'patrol');
      sea(g, 'Home Fleet', 'wapp', { dd: 60 }, 'hunt');
      if (patrol) {
        setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
        setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
      }
      // (What the patrol alone would sink of the boats there, on average, as patrolSubs counts it before the cap.)
      const w = watchAgainst(g, wapp, 'GER');
      const expected = w ? b.ships.ss * onStation(g, b, wapp) * w.s * w.kills : 0;
      // (The submarine scene's record of the month's hunt, by sea: the ships' kills alone.)
      subScene.on = true;
      try { navalUpkeep(g, 'UK'); } finally { subScene.on = false; }
      const hunt = (g.subLook ?? []).find((l) => l.nat === 'UK' && l.turn === g.turn)?.hunts.find((h) => h.zone === wapp)?.sunk ?? 0;
      return { hunt, air: g.patrolSunk?.UK?.seas[wapp] ?? 0, lost: 40 - b.ships.ss, expected };
    };
    // (The cap: 'most' of the boats a fleet had at the month's start, an expected count, rolled whole. Here a whole
    // number, so the room the hunt leaves is whole too.)
    const cap = FLEETS.hunt.most * 40;
    expect(Number.isInteger(cap)).toBe(true);
    for (let seed = 1; seed <= 4; seed++) {
      const without = month(seed, false), withP = month(seed, true);
      expect(without.hunt).toBeGreaterThan(0);
      expect(without.air).toBe(0);
      expect(withP.hunt).toBe(without.hunt);
      // (The patrol alone would sink more than the whole cap, let alone the room the hunt leaves: the cap is what stops
      // it, in every seed, at exactly that room.)
      expect(withP.expected).toBeGreaterThan(cap);
      expect(withP.air).toBe(cap - withP.hunt);
      expect(withP.lost).toBe(withP.hunt + withP.air);
      expect(withP.lost).toBe(cap);
    }
  });

  it('the month\'s words name each nation\'s boats sunk from the air by its own name', () => {
    lay(STRONG);
    const g = open(), wapp = zoneOf(g, 'wapp');
    declareWar(g, 'ITA', 'UK');
    grantTech(g, 'UK', 'asvradar');
    sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
    sea(g, 'Submarine Fleet', 'wapp', { ss: 20 }, 'raid', undefined, 'ITA');
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    navalUpkeep(g, 'UK');
    const rec = g.patrolSunk!.UK;
    expect(rec.sunk.GER).toBeGreaterThan(0);
    expect(rec.sunk.ITA).toBeGreaterThan(0);
    const said = g.headlines.filter((x) => /aircraft on patrol sink/.test(x.text)).map((x) => x.text);
    expect(said).toEqual([`British aircraft on patrol sink ${rec.sunk.GER} German and ${rec.sunk.ITA} Italian submarines in the Western Approaches.`]);
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

});

describe('the aircraft lost on patrol: an operational share where it is laid, and the enemy\'s fighters where they meet it, and nothing else (Q6 (a))', () => {
  it('with the operational share unread and no enemy fighter in reach, no bomber is lost: an open sea has no guns', () => {
    lay();
    expect(FIG.lost).toBeNull();
    for (let seed = 1; seed <= 5; seed++) {
      const g = open(seed), wapp = zoneOf(g, 'wapp'), grp = group(g, 'No. 1 Group');
      quiet(g);
      sea(g, 'U-boat Fleet', 'wapp', { ss: 10 }, 'raid');
      setMission(g, grp, 'seapatrol', wapp);
      // (A strike there would still take the guns' background share; the patrol does not.)
      const a = assessAir(g, [grp], 'seapatrol', wapp);
      expect(a.def).toEqual([]);
      expect(a.gunLoss).toBeGreaterThan(0);
      expect(patrolLoss(g, grp, wapp)).toEqual({ op: 0, fighters: 0 });
      const was = grp.bmr;
      navalUpkeep(g, 'UK');
      expect(grp.bmr).toBe(was);
      expect(grp.lost.bmr).toBe(0);
    }
  });

  it('laid, the operational share falls on the aircraft on patrol, not the whole group, and is booked to the group\'s and the nation\'s losses as a raid\'s are', () => {
    lay({ ...FIG, lost: 0.2 });
    const g = open(), wapp = zoneOf(g, 'wapp'), grp = group(g, 'No. 1 Group');
    quiet(g);
    sea(g, 'U-boat Fleet', 'wapp', { ss: 10 }, 'raid');
    setMission(g, grp, 'seapatrol', wapp);
    grp.ready = 0.5;
    const A = patrolAircraft(g, grp, wapp);
    expect(A).toBeCloseTo(grp.bmr / 2, 9);
    const x = patrolLoss(g, grp, wapp);
    expect(x.op).toBeCloseTo(0.2 * A, 9);
    expect(x.fighters).toBe(0);
    const was = grp.bmr, pool = airPool(g, 'UK').lostBmr;
    navalUpkeep(g, 'UK');
    const k = was - grp.bmr;
    expect(k).toBeGreaterThanOrEqual(Math.floor(0.2 * A));
    expect(k).toBeLessThanOrEqual(Math.ceil(0.2 * A));
    expect(grp.lost.bmr).toBe(k);
    expect(airPool(g, 'UK').lostBmr - pool).toBe(k);
  });

  it('where enemy fighters meet it, it loses what they take, as a strike there would, and not the strike\'s guns\' share', () => {
    lay();
    const g = open(), nsea = zoneOf(g, 'nsea'), grp = group(g, 'No. 1 Group');
    quiet(g);
    foe(g);
    expect(setMission(g, grp, 'seapatrol', nsea)).toBe(true);
    const a = assessAir(g, [grp], 'seapatrol', nsea);
    expect(a.def.length).toBeGreaterThan(0);
    const fighters = a.B * (a.bomberLoss - a.gunLoss);
    expect(fighters).toBeGreaterThan(1);
    const x = patrolLoss(g, grp, nsea);
    expect(x.op).toBe(0);
    expect(x.fighters).toBeCloseTo(fighters, 9);
    const was = grp.bmr;
    navalUpkeep(g, 'UK');
    expect(was - grp.bmr).toBeGreaterThanOrEqual(Math.floor(fighters));
    expect(was - grp.bmr).toBeLessThanOrEqual(Math.ceil(fighters));
    expect(grp.lost.bmr).toBe(was - grp.bmr);
  });

  it('live with the air fighting\'s row unread, the patrol works, and no enemy fighter meets it or takes a bomber (the design director\'s ruling of 17:49Z, 5 (a))', () => {
    lay({ ...STRONG, intensity: null });
    const g = open(), nsea = zoneOf(g, 'nsea'), grp = group(g, 'No. 1 Group');
    expect(patrolOn(g)).toBe(true);
    expect(patrolMet()).toBe(false);
    quiet(g);
    foe(g);
    grantTech(g, 'UK', 'asvradar');
    const boats = sea(g, 'U-boat Fleet', 'nsea', { ss: 40 }, 'raid');
    expect(setMission(g, grp, 'seapatrol', nsea)).toBe(true);
    // (Over the North Sea in 1939 the German fighters would meet it; with the row unread none comes up.)
    expect(assessAir(g, [grp], 'seapatrol', nsea).def).toEqual([]);
    expect(patrolLoss(g, grp, nsea)).toEqual({ op: 0, fighters: 0 });
    const was = grp.bmr;
    navalUpkeep(g, 'UK');
    expect(grp.bmr).toBe(was);
    expect(grp.lost.bmr).toBe(0);
    // (The patrol itself works: it watches the sea and sinks boats there.)
    expect(watchAgainst(g, nsea, 'GER')!.s).toBeGreaterThan(0);
    expect(g.patrolSunk!.UK.seas[nsea] ?? 0).toBeGreaterThan(0);
    expect(boats.ships.ss).toBeLessThan(40);
    // (The row laid, the same fighters meet it and take their part.)
    CAUSE.patrol.intensity = 0.5;
    expect(patrolMet()).toBe(true);
    expect(assessAir(g, [grp], 'seapatrol', nsea).def.length).toBeGreaterThan(0);
    expect(patrolLoss(g, grp, nsea).fighters).toBeGreaterThan(0);
  });
});

describe('the computer\'s staffs (Q7, Q5)', () => {
  beforeEach(() => lay({ ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } }));

  it('send no group where its side has seen no enemy boat, and one where it saw many (the design director\'s ruling of 17:54Z)', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    const mine = g.airFleets.filter((f) => f.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol')).toBe(false);
    // (Sixty boats on station, unseen: the staff never reads where they truly are.)
    sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol')).toBe(false);
    // (Its convoys met them there this month.)
    sawAll(g, 'UK');
    expect(seenBoats(g, 'UK', wapp)!.n).toBeGreaterThan(0);
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol' && f.target === wapp)).toBe(true);
    expect(mine.filter((f) => f.mission === 'seapatrol').every((f) => f.kind === 'bmr' && f.ship === undefined)).toBe(true);
  });

  it('send only groups with no fighters: a patrol flies alone, and nothing that guards the ground is taken (the air thread\'s Q7)', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    sawAll(g, 'UK');
    const mine = g.airFleets.filter((f) => f.nat === 'UK');
    for (const f of mine) if (f.kind !== 'ftr' && f.bmr > 0) f.ftr = 5;
    patrolStaff(g, 'UK', mine);
    expect(mine.some((f) => f.mission === 'seapatrol' && f.target === wapp)).toBe(false);
  });

  it('a computer-run raider moves out of waters where its boats met aircraft without kit, to a sea of the route where its boats stay up longer', () => {
    lay();
    const set = (human: string | null): { g: Game; f: Fleet; wapp: number } => {
      const g = open(), wapp = zoneOf(g, 'wapp');
      g.human = human;
      const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
      f.duty = 'raid'; f.station = wapp; f.tied = false;
      expect(quieterBeat(g, f)).toBe(-1);
      setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
      setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
      // (Until its boats have met them, the staff does not know the patrols are there: ordered is not met.)
      expect(quieterBeat(g, f)).toBe(-1);
      noteSeaSight(g, 'GER');
      expect(quieterBeat(g, f)).toBe(-1);
      // (Britain's month flies them over the boats; at the close of Germany's month at sea its boats on station in the
      // Western Approaches have met the watch there.)
      navalUpkeep(g, 'UK');
      noteSeaSight(g, 'GER');
      return { g, f, wapp };
    };
    const { g, f, wapp } = set(null);
    const to = quieterBeat(g, f);
    expect(to).toBeGreaterThanOrEqual(0);
    expect(atlantic.zones.map((k) => zoneOf(g, k))).toContain(to);
    expect(onWatch(g, f, to)).toBeGreaterThan(onWatch(g, f, wapp));
    // (Its staff makes the move, and says so.)
    navalAI(g, 'GER');
    expect(f.station).toBe(to);
    expect(f.zone).toBe(to);
    expect(g.headlines.some((x) => /shifts its hunting ground from the Western Approaches/.test(x.text))).toBe(true);
    // (A navy its player commands by hand is his to move: the staff leaves it on its beat.)
    const hand = set('GER');
    expect(byHand(hand.g, 'GER', 'navy')).toBe(true);
    expect(quieterBeat(hand.g, hand.f)).toBe(to);
    navalAI(hand.g, 'GER');
    expect(hand.f.station).toBe(wapp);
    expect(hand.f.zone).toBe(wapp);
  });

  it('a group whose own raid is worth more flies its raid, and the next best placed goes on patrol in its stead', () => {
    // (Through the whole air staff: the raid staff chooses each raid and the patrol staff weighs it before it flies (the
    // design director's ruling of 17:49Z, 2 (a)). Its share of groups raised to all of them, so that No. 1 Group is
    // given a raid each month.)
    AIRWAR.ai.raids.share = 1;
    const month = (big: number): { g: Game; n1: AirFleet; n2: AirFleet; wapp: number } => {
      const g = open(), wapp = zoneOf(g, 'wapp');
      g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
      const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
      f.duty = 'raid'; f.station = wapp; f.tied = false;
      const n1 = group(g, 'No. 1 Group'), n2 = group(g, 'No. 2 Group');
      n1.bmr = big; n1.est.bmr = big;
      // (No. 2 Group in Ulster: the Western Approaches in its reach, and no enemy works to bomb.)
      n2.base = g.regions.find((r) => r.name === 'Ulster')!.id;
      expect(g.regions.some((r) => r.ctrl === 'GER' && r.fac > 0 && whyNotMission(g, n2, 'bomb', r.id) === null)).toBe(false);
      const mine = g.airFleets.filter((x) => x.nat === 'UK');
      for (const x of mine) if (x !== n1 && x !== n2 && x.kind !== 'ftr') x.mission = 'rest';
      expect(whyNotMission(g, n1, 'seapatrol', wapp)).toBeNull();
      expect(whyNotMission(g, n2, 'seapatrol', wapp)).toBeNull();
      sawAll(g, 'UK');
      airAI(g, 'UK');
      return { g, n1, n2, wapp };
    };
    // (A raid of its usual size: No. 1 Group gives up the raid it was given and patrols where the boats are.)
    const usual = month(140);
    expect(usual.n1.mission).toBe('seapatrol');
    expect(usual.n1.target).toBe(usual.wapp);
    expect(usual.n1.flew).not.toBe(usual.g.turn);
    // (Its raid made far bigger: the best placed group flies it, and the staff sends the next.)
    const big = month(2000);
    expect(patrolAircraft(big.g, big.n1, big.wapp)).toBeGreaterThan(patrolAircraft(big.g, big.n2, big.wapp));
    expect(big.n1.flew).toBe(big.g.turn);
    expect(isOffensive(big.n1.mission)).toBe(true);
    expect(big.n2.mission).toBe('seapatrol');
    expect(big.n2.target).toBe(big.wapp);
  });

  it('after the raid staff: a group is weighed against the raid that staff chose for it, at that raid\'s own score, and gives it up only for a patrol worth more (the design director\'s ruling of 17:49Z, 2 (a))', () => {
    // (Sixty boats in the Western Approaches; No. 1 Group of 300 bombers and No. 2 Group of 140, both in Lincolnshire,
    // and every group given a raid. No. 1 Group's staff chooses a night raid worth more than its patrol, so it flies it;
    // No. 2 Group's chosen raid is worth less than its patrol, so it gives the raid up and patrols. Weighed against a
    // bombing raid re-scored apart from the raid staff, No. 1 Group was sent on patrol instead.)
    lay({ ...FIG, attacks: { kit: 2, none: 2 }, kill: { kit: 1, none: 1 } });
    AIRWAR.ai.raids.share = 1;
    const g = open(), wapp = zoneOf(g, 'wapp');
    // (The join onto release 2's guns, 25 November 2026. A war now opens with heavy guns round each great power's
    // capital and two chief factory towns (ruling 124), and this month was measured against bare ground. With Berlin,
    // the Ruhr and Bohemia gunned the staff's raids are worth less and the two groups change places: No. 1 Group
    // patrols and No. 2 Group flies the night raid. So the start guns are taken away here, as arsenal.test.ts does.)
    for (const k of Object.keys(g.flags)) if (k.startsWith('wk:aa:')) delete g.flags[k];
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    const n1 = group(g, 'No. 1 Group'), n2 = group(g, 'No. 2 Group');
    n1.bmr = 300; n1.est.bmr = 300;
    // (Each group's patrol is worth more than a raid must be worth to be flown at all, so the old floor plays no part.)
    const B = f.ships.ss * onWatch(g, f, wapp);
    expect(patrolWorth(g, 'UK', n1, wapp, atlantic, B, B)).toBeGreaterThan(AIRWAR.ai.raids.worth);
    expect(patrolWorth(g, 'UK', n2, wapp, atlantic, B, B)).toBeGreaterThan(AIRWAR.ai.raids.worth);
    sawAll(g, 'UK');
    airAI(g, 'UK');
    expect(n1.flew).toBe(g.turn);
    expect(n1.mission).toBe('night');
    expect(n2.mission).toBe('seapatrol');
    expect(n2.flew).not.toBe(g.turn);
  });

  it('with the start guns standing a raid is worth less, and each group takes the higher of its patrol and its best raid: on these figures No. 1 Group patrols (the rule working; the companion of the test above)', () => {
    // (The month of the test above, once on bare ground as there and once as a war now opens, with guns round each great
    // power's capital and two chief factory provinces (ruling 124). The design director's ask of 25 November 2026 02:38Z,
    // as he ruled it at 04:04Z: pin the cause, not the city. So three things: the guns lower a raid's worth; each group
    // takes the higher of its patrol and its best raid; and on these figures No. 1 Group patrols. A group's best raid is
    // the raid the raid staff flies for it when it is its side's only bomber group fit to fly and no patrol is weighed.)
    lay({ ...FIG, attacks: { kit: 2, none: 2 }, kill: { kit: 1, none: 1 } });
    AIRWAR.ai.raids.share = 1;
    type Month = { g: Game; n1: AirFleet; n2: AirFleet; wapp: number; B: number };
    const month = (guns: boolean, only?: string): Month => {
      const g = open(), wapp = zoneOf(g, 'wapp');
      if (!guns) for (const k of Object.keys(g.flags)) if (k.startsWith('wk:aa:')) delete g.flags[k];
      g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
      const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
      f.duty = 'raid'; f.station = wapp; f.tied = false;
      const n1 = group(g, 'No. 1 Group'), n2 = group(g, 'No. 2 Group');
      n1.bmr = 300; n1.est.bmr = 300;
      // (For a group's best raid: every other bomber group of its side has flown this month already, and stays away.)
      if (only) for (const x of g.airFleets) if (x.nat === 'UK' && x.kind !== 'ftr' && x.name !== only) { x.mission = 'rest'; x.flew = g.turn; }
      return { g, n1, n2, wapp, B: f.ships.ss * onWatch(g, f, wapp) };
    };
    // (The month flown by the whole air staff; with no patrol weighed, by the raid staff alone.)
    const fly = (m: Month, patrol: boolean): void => {
      sawAll(m.g, 'UK');
      if (!patrol) CAUSE.patrol.live = false;
      try { airAI(m.g, 'UK'); } finally { CAUSE.patrol.live = true; }
    };
    // (The raid staff's own score of a raid flown by the group alone (air.ts raidStaff, for a computer's staff): what it
    // should do, counted in aircraft, less the aircraft it should cost.)
    const score = (g: Game, f: AirFleet, mission: 'night', t: number): number => {
      const a = assessAir(g, [f], mission, t);
      expect(a.why).toBeNull();
      const fc = forecastAir(g, a, mission, t);
      return fc.worth + fc.foeLoss - fc.ftrLoss - fc.bmrLoss;
    };
    // (A group's best raid on either ground, scored in the month as it stands before its staff sits. On these figures
    // each group's best raid is a night raid, flown alone.)
    const best = (guns: boolean, name: string, on: Month, f: AirFleet): { t: number; v: number } => {
      const alone = month(guns, name), g = group(alone.g, name);
      fly(alone, false);
      expect(g.flew).toBe(alone.g.turn);
      expect(g.mission).toBe('night');
      return { t: g.target, v: score(on.g, f, 'night', g.target) };
    };
    // (A group's patrol as its staff weighs it: over the boats its side expects in the Western Approaches, less those the
    // groups sent there before it keep down, on top of their watch (seapatrol.ts patrolStaff, boatsExpected).)
    const patrol = (m: Month, f: AirFleet, before: AirFleet[] = []): number => {
      const was = before.map((x) => ({ x, mission: x.mission, target: x.target }));
      for (const x of before) { x.mission = 'seapatrol'; x.target = m.wapp; }
      try {
        const w = watchFor(m.g, m.wapp, ukSide(m.g)), B = m.B * (w && w.none > 0 ? 1 - w.none : 1);
        return patrolWorth(m.g, 'UK', f, m.wapp, atlantic, B, B);
      } finally { for (const o of was) { o.x.mission = o.mission; o.x.target = o.target; } }
    };
    const bare = month(false), gunned = month(true);
    const raid = {
      bare: { n1: best(false, 'No. 1 Group', bare, bare.n1) },
      gunned: { n1: best(true, 'No. 1 Group', gunned, gunned.n1), n2: best(true, 'No. 2 Group', gunned, gunned.n2) },
    };
    // (No. 1 Group is weighed first; on the guns' ground No. 2 Group is weighed next, with No. 1 Group on patrol.)
    const worth = { bare: { n1: patrol(bare, bare.n1) }, gunned: { n1: patrol(gunned, gunned.n1), n2: patrol(gunned, gunned.n2, [gunned.n1]) } };
    // (1) The guns lower a raid's worth. They stand where ruling 124 puts them, round the capital and the chief factory
    // provinces; a night raid on each gunned place is worth less than on the same place bare, and so is No. 1 Group's
    // best raid. A patrol is worth the same on either ground: the guns change only the raids.
    const town = (g: Game, name: string): number => g.regions.find((r) => r.name === name)!.id;
    const gunsAt = (g: Game, t: number): number => g.flags[`wk:aa:${t}`] ?? 0;
    const gunnedTowns = ['Berlin', 'Ruhr'];
    for (const name of gunnedTowns) {
      const t = town(gunned.g, name);
      expect(gunsAt(gunned.g, t)).toBeGreaterThan(0);
      expect(gunsAt(bare.g, t)).toBe(0);
      expect(score(gunned.g, gunned.n1, 'night', t)).toBeLessThan(score(bare.g, bare.n1, 'night', t));
    }
    expect(raid.gunned.n1.v).toBeLessThan(raid.bare.n1.v);
    expect(worth.bare.n1).toBeCloseTo(worth.gunned.n1, 9);
    for (const m of [bare, gunned]) fly(m, true);
    // (2) Each group takes the higher of its patrol and its best raid: one worth more than its patrol is flown, and a
    // patrol worth more than its best raid is flown instead. (On bare ground only No. 1 Group is weighed here: No. 2 Group
    // is weighed there after No. 1 Group's raid has taken its target, and the test above has that month.)
    const took = (m: Month, f: AirFleet, r: { t: number; v: number }, w: number): void => {
      if (r.v > w) {
        expect(f.flew).toBe(m.g.turn);
        expect(f.mission).toBe('night');
        expect(f.target).toBe(r.t);
      } else {
        expect(f.mission).toBe('seapatrol');
        expect(f.target).toBe(m.wapp);
        expect(f.flew).not.toBe(m.g.turn);
      }
    };
    took(bare, bare.n1, raid.bare.n1, worth.bare.n1);
    took(gunned, gunned.n1, raid.gunned.n1, worth.gunned.n1);
    took(gunned, gunned.n2, raid.gunned.n2, worth.gunned.n2);
    // (3) On these figures: on bare ground No. 1 Group's best raid is worth more than its patrol and it flies it; with the
    // guns standing its best raid is worth less, and it patrols.
    expect(raid.bare.n1.v).toBeGreaterThan(worth.bare.n1);
    expect(raid.gunned.n1.v).toBeLessThan(worth.gunned.n1);
    expect(gunned.n1.mission).toBe('seapatrol');
  });

  it('a group the raid staff leaves on support is weighed against nothing: with no raid flown this month, it patrols (17:49Z, 2 (a))', () => {
    // (The raid staff's share set to none: it flies no raid. No. 1 Group, raised to 300 bombers, then patrols, though a
    // raid of its own re-scored apart from the staff would have been worth more than its patrol.)
    lay({ ...FIG, attacks: { kit: 2, none: 2 }, kill: { kit: 0.3, none: 0.3 } });
    AIRWAR.ai.raids.share = 0;
    const g = open(), wapp = zoneOf(g, 'wapp');
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    const n1 = group(g, 'No. 1 Group');
    n1.bmr = 300; n1.est.bmr = 300;
    for (const x of g.airFleets) if (x.nat === 'UK' && x !== n1 && x.kind !== 'ftr') x.mission = 'rest';
    const B = f.ships.ss * onWatch(g, f, wapp);
    const v = patrolWorth(g, 'UK', n1, wapp, atlantic, B, B);
    expect(v).toBeGreaterThan(AIRWAR.ai.raids.worth);
    sawAll(g, 'UK');
    airAI(g, 'UK');
    expect(g.airFleets.some((x) => x.nat === 'UK' && x.flew === g.turn && isOffensive(x.mission))).toBe(false);
    expect(n1.mission).toBe('seapatrol');
    expect(n1.target).toBe(wapp);
  });

  it('a group left on support whose patrol is worth nothing is passed over, and the next best placed is sent', () => {
    // (The support phase: no raid is weighed. No. 1 Group, of 600 bombers, is the best placed, but on patrol it would lose
    // more to the operational share than its patrol would gain; No. 2 Group's 20 would gain more than they lose.)
    lay({ ...FIG, lost: 0.02 });
    const g = open(), wapp = zoneOf(g, 'wapp');
    quiet(g);
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    const n1 = group(g, 'No. 1 Group'), n2 = group(g, 'No. 2 Group');
    n1.bmr = 600; n1.est.bmr = 600;
    n2.bmr = 20; n2.est.bmr = 20;
    const mine = g.airFleets.filter((x) => x.nat === 'UK');
    for (const x of mine) if (x !== n1 && x !== n2 && x.kind !== 'ftr') x.mission = 'rest';
    expect(patrolAircraft(g, n1, wapp)).toBeGreaterThan(patrolAircraft(g, n2, wapp));
    const B = f.ships.ss * onWatch(g, f, wapp);
    expect(patrolWorth(g, 'UK', n1, wapp, atlantic, B, B)).toBeLessThanOrEqual(0);
    expect(patrolWorth(g, 'UK', n2, wapp, atlantic, B, B)).toBeGreaterThan(0);
    sawAll(g, 'UK');
    expect(patrolStaff(g, 'UK', mine)).toBe(true);
    expect(n2.mission).toBe('seapatrol');
    expect(n2.target).toBe(wapp);
    expect(n1.mission).toBe('support');
  });

  it('a patrol goes wherever it is worth more than the raid it would replace, with no floor besides (the design director\'s ruling of 17:49Z, 3 (a))', () => {
    // (No. 2 Group in Ulster, with no enemy works in its reach and so no raid to give up; its patrol worth less than
    // what a raid must be worth to be flown, but more than nothing.)
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp');
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    const n2 = group(g, 'No. 2 Group');
    n2.base = g.regions.find((r) => r.name === 'Ulster')!.id;
    expect(g.regions.some((r) => r.ctrl === 'GER' && r.fac > 0 && whyNotMission(g, n2, 'bomb', r.id) === null)).toBe(false);
    const mine = g.airFleets.filter((x) => x.nat === 'UK');
    for (const x of mine) if (x !== n2 && x.kind !== 'ftr') x.mission = 'rest';
    const B = f.ships.ss * onWatch(g, f, wapp);
    const v = patrolWorth(g, 'UK', n2, wapp, atlantic, B, B);
    expect(v).toBeGreaterThan(0);
    expect(v).toBeLessThan(AIRWAR.ai.raids.worth);
    sawAll(g, 'UK');
    const sent = patrolStaff(g, 'UK', mine);
    expect(n2.mission).toBe('seapatrol');
    expect(n2.target).toBe(wapp);
    expect(sent).toBe(true);
  });

  it('weighs what a patrol would lose, bomber for bomber as a raid\'s losses: the operational share on the aircraft on patrol, and the enemy fighters\' part', () => {
    const g = open(), grp = group(g, 'No. 1 Group');
    quiet(g);
    grp.ready = 0.5;
    // (The worth of one more group over a sea for 20 boats expected there. The route's cargoes are not read: none has
    // been lost on it yet.)
    const worth = (z: number): number => patrolWorth(g, 'UK', grp, z, atlantic, 20, 20);
    // (The operational share, on the aircraft on patrol: the group's bombers at half readiness, not all of them.)
    const wapp = zoneOf(g, 'wapp');
    const free = worth(wapp);
    expect(free).toBeGreaterThan(0);
    lay({ ...STRONG, lost: 0.2 });
    expect(worth(wapp)).toBeCloseTo(free - 0.2 * patrolAircraft(g, grp, wapp), 9);
    expect(0.2 * patrolAircraft(g, grp, wapp)).toBeCloseTo(0.1 * grp.bmr, 9);
    // (Enemy fighters over the sea, the North Sea in 1939: less by what they would take.)
    lay(STRONG);
    const nsea = zoneOf(g, 'nsea');
    const calm = worth(nsea);
    expect(patrolLoss(g, grp, nsea).fighters).toBe(0);
    foe(g);
    const x = patrolLoss(g, grp, nsea);
    expect(x.fighters).toBeGreaterThan(0);
    expect(worth(nsea)).toBeCloseTo(calm - x.fighters, 9);
  });
});

describe('a raider keeps its beat unless the move\'s gain covers what the move loses (the design director\'s ruling of 17:49Z, 1 (a))', () => {
  /**
   * Germany holding the Atlantic coast of France and the ports of Norway, its U-boat Fleet of 60 based at Bordeaux on
   * the Western Approaches, and the two staffs month after month, each reading only what its side has seen (the design
   * director's ruling of 17:54Z): Britain's convoys meet the boats where they raid and its patrol staff sends its groups
   * where its side last saw them; Germany's boats meet the watch kept over their beat (metBeat), and its naval staff then
   * moves them (navalAI). Strong figures and a0 8, so that the patrol keeps the boats down hard.
   * Each month's beat, base and whether the boats are in harbour, and the watch over them.
   */
  function months(n: number): { g: Game; f: Fleet; log: { beat: number; port: string; harbour: boolean; watched: number }[] } {
    lay({ ...STRONG, a0: 8 });
    const g = open();
    quiet(g);
    for (const r of g.regions) if (['Brittany', 'Nantes', 'Poitou', 'Bordeaux', 'Bergen', 'Trondheim'].includes(r.name)) { r.ctrl = 'GER'; r.since = -5; }
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    const home = g.byName.Bordeaux;
    f.base = home.id; f.port = home.name; f.home = home.seas[0];
    f.duty = 'raid'; f.station = f.zone; f.tied = false;
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const log: { beat: number; port: string; harbour: boolean; watched: number }[] = [];
    for (let m = 0; m < n; m++) {
      sawAll(g, 'UK');
      const mine = g.airFleets.filter((x) => x.nat === 'UK');
      for (const x of mine) if (x.flew !== g.turn) { x.mission = 'support'; x.target = -1; }
      patrolStaff(g, 'UK', mine);
      const watched = f.stance === 'port' ? 0 : watchAgainst(g, f.station, 'GER')?.s ?? 0;
      metBeat(g, f);
      navalAI(g, 'GER');
      log.push({ beat: f.station, port: f.port, harbour: f.stance === 'port', watched });
      g.turn++;
    }
    return { g, f, log };
  }

  it('the boats stay on a beat where a quieter sea\'s gain does not cover the crossing to it, nor a new base\'s the month in harbour', () => {
    lay({ ...STRONG, a0: 8 });
    const g = open();
    quiet(g);
    for (const r of g.regions) if (['Brittany', 'Nantes', 'Poitou', 'Bordeaux', 'Bergen', 'Trondheim'].includes(r.name)) { r.ctrl = 'GER'; r.since = -5; }
    const wapp = zoneOf(g, 'wapp'), irish = zoneOf(g, 'irish'), natl = zoneOf(g, 'natl');
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    const home = g.byName.Bordeaux;
    f.base = home.id; f.port = home.name; f.home = home.seas[0];
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    metEvery(g);
    const up = onWatch(g, f, wapp), D = CAUSE.station.ss!;
    // (The port the boats' staff would raid a beat from: nearerPort, read with the beat set there.)
    const port = (z: number): string | undefined => { const keep = f.station; f.station = z; try { return nearerPort(g, f)?.name; } finally { f.station = keep; } };
    expect(port(wapp)).toBeUndefined();
    // (The Irish Sea, unwatched and two seas from Bordeaux, keeps the boats up longer than the watched Western
    // Approaches, one sea out; but the boats on station there would lose the crossing to it, a sea of passage.)
    const irishGain = onWatch(g, f, irish) - up;
    expect(irishGain).toBeGreaterThan(0);
    expect(port(irish)).toBeUndefined();
    expect(irishGain).toBeLessThan((up * D.passage! * 1) / 30);
    // (The North Atlantic is one sea from Bergen, and the staff would base the boats there: more time up, but not
    // enough to pay for the month in harbour the move of base costs.)
    expect(port(natl)).toBe('Bergen');
    const bergen = g.byName.Bergen;
    const natlGain = onWatch(g, { nat: 'GER', base: bergen.id, home: bergen.seas[0] }, natl) - up;
    expect(natlGain).toBeGreaterThan(0);
    expect(natlGain).toBeLessThan(up);
    expect(quieterBeat(g, f)).toBe(-1);
  });

  it('the patrol and the boats no longer chase each other: the boats keep their beat and their base, never in harbour, and the patrol stays over them', () => {
    // (Before the ruling the boats moved on each month the patrol came, between the Western Approaches, the Irish Sea
    // and the North Atlantic, rebasing between Bordeaux, Bergen and Brittany, and spent those months in harbour.)
    const { g, log } = months(8);
    const wapp = zoneOf(g, 'wapp');
    expect(log.every((m) => m.beat === wapp && m.port === 'Bordeaux' && !m.harbour)).toBe(true);
    expect(log.every((m) => m.watched > 0)).toBe(true);
  });

  it('a move whose gain covers the crossing still goes ahead', () => {
    // (The raider of the test above, based in Germany: the North Atlantic is a sea nearer its port than the Western
    // Approaches, and the gain there covers the crossing.)
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    setMission(g, group(g, 'No. 2 Group'), 'seapatrol', wapp);
    metEvery(g);
    const up = onWatch(g, f, wapp), D = CAUSE.station.ss!;
    f.station = natl;
    expect(nearerPort(g, f)).toBeNull();
    f.station = wapp;
    expect(onWatch(g, f, natl) - up).toBeGreaterThan((up * D.passage! * 1) / 30);
    expect(quieterBeat(g, f)).toBe(natl);
  });

  // Each part of what a move loses, and the beat it is weighed against. Strong figures and a0 8, no German fighter up,
  // and the U-boat Fleet of 40 at sea on its beat in the Western Approaches, based at its own port, Wilhelmshaven, unless
  // another is named; Germany holds the ports named since last month.
  function raider(g: Game, held: string[], base?: string): Fleet {
    quiet(g);
    for (const r of g.regions) if (held.includes(r.name)) { r.ctrl = 'GER'; r.since = g.turn - 1; }
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 40 }, 'raid');
    if (base) { const b = g.byName[base]; f.base = b.id; f.port = b.name; f.home = b.seas[0]; }
    f.duty = 'raid'; f.station = f.zone; f.tied = false;
    return f;
  }
  /** Puts a group on patrol over a sea with so many bombers at full readiness, from another airfield where named. */
  function patrolWith(g: Game, name: string, z: string, bmr: number, at?: string): void {
    const x = group(g, name);
    if (at) x.base = g.byName[at].id;
    x.bmr = bmr; x.est.bmr = bmr; x.ready = 1;
    expect(setMission(g, x, 'seapatrol', zoneOf(g, z))).toBe(true);
  }
  /** The port the boats' staff would raid a beat from (nearerPort, read with the beat set there): undefined for their own. */
  const portFor = (g: Game, f: Fleet, z: number): string | undefined => { const keep = f.station; f.station = z; try { return nearerPort(g, f)?.name; } finally { f.station = keep; } };
  /** The boats' time up on station in a sea, raiding from a port. */
  const upFrom = (g: Game, port: string, z: number): number => { const r = g.byName[port]; return onWatch(g, { nat: 'GER', base: r.id, home: r.seas[0] }, z); };

  it('a move that keeps the boats\' base is spared the month in harbour that keeping their beat would cost: they move, rather than lie a month in port', () => {
    // (Germany has held Brittany since last month. Kept on the Western Approaches, the boats would be based there, a sea
    // nearer, and lie the month in harbour. The Irish Sea, unwatched, is as far from Wilhelmshaven as the Western
    // Approaches: the boats sail there from their own port, losing only the crossing.)
    lay({ ...STRONG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), irish = zoneOf(g, 'irish');
    const f = raider(g, ['Brittany']);
    patrolWith(g, 'No. 1 Group', 'wapp', 140);
    patrolWith(g, 'No. 2 Group', 'natl', 140);
    metEvery(g);
    expect(portFor(g, f, wapp)).toBe('Brittany');
    expect(portFor(g, f, irish)).toBeUndefined();
    // (Weighed against the beat as kept from Brittany, the Irish Sea's gain is less than the crossing; but keeping the
    // beat costs the month in harbour, and the move does not.)
    const D = CAUSE.station.ss!, gain = onWatch(g, f, irish) - upFrom(g, 'Brittany', wapp);
    expect(gain).toBeGreaterThan(0);
    expect(gain).toBeLessThan((onWatch(g, f, wapp) * D.passage! * 1) / 30);
    expect(quieterBeat(g, f)).toBe(irish);
    navalAI(g, 'GER');
    expect(f.station).toBe(irish);
    expect(f.port).toBe('Wilhelmshaven');
    expect(f.stance).toBe('raid');
    expect(atSea(f)).toBe(true);
    // (And there they stay: no aircraft watch the Irish Sea, and no port is nearer it.)
    expect(quieterBeat(g, f)).toBe(-1);
    expect(nearerPort(g, f)).toBeNull();
  });

  it('a fleet in harbour sails to a new beat as to its own, so it moves for any gain; at sea on its beat, the same gain does not cover the crossing', () => {
    lay({ ...STRONG, a0: 8 });
    const set = (harbour: boolean): { g: Game; f: Fleet } => {
      const g = open();
      const f = raider(g, []);
      if (harbour) { f.stance = 'port'; f.zone = f.home; }
      patrolWith(g, 'No. 1 Group', 'wapp', 20);
      patrolWith(g, 'No. 2 Group', 'natl', 60);
      metEvery(g);
      return { g, f };
    };
    const D = CAUSE.station.ss!;
    const port = set(true), wapp = zoneOf(port.g, 'wapp'), natl = zoneOf(port.g, 'natl');
    expect(atSea(port.f)).toBe(false);
    expect(quieterBeat(port.g, port.f)).toBe(natl);
    // (A gain less than the crossing from its port's sea would be to a fleet at sea there, and less than the crossing
    // from its beat.)
    const gain = onWatch(port.g, port.f, natl) - onWatch(port.g, port.f, wapp);
    expect(gain).toBeGreaterThan(0);
    expect(gain).toBeLessThan((onWatch(port.g, port.f, port.f.zone) * D.passage! * passageSeas(port.g, { nat: 'GER', base: -1, home: port.f.zone }, natl)) / 30);
    expect(gain).toBeLessThan((onWatch(port.g, port.f, wapp) * D.passage! * 1) / 30);
    const out = set(false);
    expect(atSea(out.f)).toBe(true);
    expect(quieterBeat(out.g, out.f)).toBe(-1);
  });

  it('a move of base goes ahead where its gain, read from the port the boats would be based at, covers the month in harbour; read from their own port it would not', () => {
    // (June, long-range patrol aircraft, and heavy patrols over the Western Approaches and the Irish Sea: the boats at
    // Bordeaux keep under half their time up on their beat. The North Atlantic is unwatched, and the boats' staff would
    // base them at Bergen to raid it, a sea nearer than Bordeaux.)
    lay({ ...STRONG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    while (turnToDate(g.turn).month !== 5) g.turn++;
    grantTech(g, 'UK', 'patrolaircraft');
    const f = raider(g, ['Bordeaux', 'Bergen'], 'Bordeaux');
    patrolWith(g, 'No. 1 Group', 'wapp', 400);
    patrolWith(g, 'No. 2 Group', 'irish', 300);
    metEvery(g);
    expect(portFor(g, f, wapp)).toBeUndefined();
    expect(portFor(g, f, natl)).toBe('Bergen');
    // (The month in harbour: the month's whole time up on the beat, given up.)
    const up = onWatch(g, f, wapp);
    expect(upFrom(g, 'Bergen', natl) - up).toBeGreaterThan(up);
    expect(onWatch(g, f, natl) - up).toBeLessThan(up);
    expect(quieterBeat(g, f)).toBe(natl);
    navalAI(g, 'GER');
    expect(f.station).toBe(natl);
    expect(f.port).toBe('Bergen');
    expect(f.stance).toBe('port');
  });

  it('where keeping the beat would move the boats\' base too, a move of base is charged nothing for the month in harbour, which comes either way', () => {
    // (A German landing in Scotland: Germany holds the Highlands as well as Brittany. Kept on the Western Approaches, the
    // boats would be based in Brittany; moved to the North Atlantic, in the Highlands, on its coast.)
    lay({ ...STRONG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    const f = raider(g, ['Brittany', 'Highlands']);
    patrolWith(g, 'No. 1 Group', 'wapp', 20);
    metEvery(g);
    expect(portFor(g, f, wapp)).toBe('Brittany');
    expect(portFor(g, f, natl)).toBe('Highlands');
    // (The gain is less than the month in harbour, but that month is lost whichever the staff does.)
    const gain = upFrom(g, 'Highlands', natl) - upFrom(g, 'Brittany', wapp);
    expect(gain).toBeGreaterThan(0);
    expect(gain).toBeLessThan(onWatch(g, f, wapp));
    expect(quieterBeat(g, f)).toBe(natl);
    navalAI(g, 'GER');
    expect(f.station).toBe(natl);
    expect(f.port).toBe('Highlands');
  });

  it('the boats\' own beat is weighed as the staff would keep it, from the port it would base them at', () => {
    // (Germany holds Brittany and the Highlands, as above; the North Atlantic and the Irish Sea are watched harder than the
    // Western Approaches, the Irish Sea by the French air force's bombers from Wales. From the Highlands the North
    // Atlantic keeps the boats up longer than the Western Approaches from Wilhelmshaven, but not longer than from
    // Brittany, where the staff would base them to keep their beat: so they keep it.)
    lay({ ...STRONG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), irish = zoneOf(g, 'irish'), natl = zoneOf(g, 'natl');
    grantTech(g, 'UK', 'patrolaircraft');
    grantTech(g, 'FRA', 'patrolaircraft');
    const f = raider(g, ['Brittany', 'Highlands']);
    patrolWith(g, 'No. 1 Group', 'wapp', 10);
    patrolWith(g, 'No. 2 Group', 'natl', 120);
    patrolWith(g, '1st Bomber Group', 'irish', 30, 'Wales');
    metEvery(g);
    expect(portFor(g, f, wapp)).toBe('Brittany');
    expect(portFor(g, f, natl)).toBe('Highlands');
    expect(portFor(g, f, irish)).toBe('Highlands');
    const kept = upFrom(g, 'Brittany', wapp);
    expect(upFrom(g, 'Highlands', natl)).toBeGreaterThan(onWatch(g, f, wapp));
    expect(upFrom(g, 'Highlands', natl)).toBeLessThan(kept);
    expect(upFrom(g, 'Highlands', irish)).toBeLessThan(kept);
    expect(quieterBeat(g, f)).toBe(-1);
    navalAI(g, 'GER');
    expect(f.station).toBe(wapp);
    expect(f.port).toBe('Brittany');
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

describe('the staff counts the boats where its side last saw them, and predicts no move of theirs (the design director\'s ruling of 17:54Z, Q5 (a))', () => {
  beforeEach(() => lay({ ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } }));

  /**
   * The U-boat Fleet of 60 on its beat in the Western Approaches, its boats having met both British groups' watch there
   * (metEvery, the groups then stood down again), so that their own staff would move them out of it; and Britain's
   * convoys having met them there (sawAll).
   */
  function seen(g: Game): Fleet {
    const wapp = zoneOf(g, 'wapp');
    const f = sea(g, 'U-boat Fleet', 'wapp', { ss: 60 }, 'raid');
    f.duty = 'raid'; f.station = wapp; f.tied = false;
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    const groups = ['No. 1 Group', 'No. 2 Group'].map((n) => group(g, n));
    for (const x of groups) expect(setMission(g, x, 'seapatrol', wapp)).toBe(true);
    metEvery(g);
    for (const x of groups) { x.mission = 'support'; x.target = -1; }
    sawAll(g, 'UK');
    return f;
  }

  it('without kit its groups go to the sea it saw the boats in, though their own staff would move them out of the watch', () => {
    // (Before the ruling the staff read the boats' own staff's move and spread its groups over the route after them.)
    const g = open(), wapp = zoneOf(g, 'wapp');
    const f = seen(g);
    // (Their own staff would move them, on the watch its boats met; the patrol staff does not guess it.)
    const to = quieterBeat(g, f);
    expect(to).toBeGreaterThanOrEqual(0);
    expect(to).not.toBe(wapp);
    const mine = g.airFleets.filter((x) => x.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    const sent = mine.filter((x) => x.mission === 'seapatrol');
    expect(sent.length).toBeGreaterThan(1);
    expect(sent.every((x) => x.target === wapp)).toBe(true);
    expect(quieterBeat(g, f)).toBe(to);
  });

  it('a navy its player commands by hand or one the computer runs: the boats are counted where they were seen, and the same groups go to the same sea', () => {
    const run = (human: string | null): number[] => {
      const g = open(), wapp = zoneOf(g, 'wapp');
      g.human = human;
      expect(byHand(g, 'GER', 'navy')).toBe(human === 'GER');
      const f = seen(g);
      // (The staff the computer runs would move the boats; the player's he moves himself. Neither is guessed.)
      expect(quieterBeat(g, f)).toBeGreaterThanOrEqual(0);
      const mine = g.airFleets.filter((x) => x.nat === 'UK');
      patrolStaff(g, 'UK', mine);
      const sent = mine.filter((x) => x.mission === 'seapatrol').map((x) => x.target);
      expect(sent.every((z) => z === wapp)).toBe(true);
      return sent;
    };
    const hand = run('GER');
    expect(hand.length).toBeGreaterThan(1);
    expect(run(null)).toEqual(hand);
  });

  it('weighs the cargoes its patrols would save over the boats its side last saw on the route, never the count\'s boats truly on station', () => {
    // (patrolWorth's cargo term: last month's losses on the route shared over 'all', the boats seen on the route's seas.
    // The route's count of the boats truly on station, which no staff reads, does not move it.)
    const g = open(), wapp = zoneOf(g, 'wapp');
    const grp = group(g, 'No. 1 Group');
    const cargo = (all: number, truly: number): number => {
      g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05, count: { boats: truly } } as any;
      const v = patrolWorth(g, 'UK', grp, wapp, atlantic, 20, all);
      g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0 } as any;
      return v - patrolWorth(g, 'UK', grp, wapp, atlantic, 20, all);
    };
    expect(cargo(20, 60)).toBeGreaterThan(0);
    expect(cargo(20, 5)).toBeCloseTo(cargo(20, 60), 9);
    expect(cargo(40, 60)).toBeCloseTo(cargo(20, 60) / 2, 9);
  });
});

describe('the day\'s length (one function for every system that needs daylight)', () => {
  it('is astronomy: the sunrise equation over latitude and month', () => {
    // (About 12 hours and a few minutes at the equator all year; 50 N about 16.3 hours in June and 8.1 in December;
    // the midnight sun and the polar night at 70 N.)
    for (let m = 0; m < 12; m++) expect(Math.abs(dayShare(0, m) * 24 - 12.1)).toBeLessThan(0.1);
    expect(dayShare(50, 5) * 24).toBeCloseTo(16.3, 0);
    expect(dayShare(50, 11) * 24).toBeCloseTo(8.1, 0);
    expect(dayShare(70, 5)).toBe(1);
    expect(dayShare(70, 11)).toBe(0);
    expect(dayShare(-50, 11)).toBeCloseTo(dayShare(50, 5), 1);
  });

  it('reads each sea at its own mean latitude on the chart', () => {
    const g = open();
    expect(seaLatitude(g, zoneOf(g, 'wapp'))).toBeGreaterThan(47);
    expect(seaLatitude(g, zoneOf(g, 'wapp'))).toBeLessThan(51);
    expect(seaLatitude(g, zoneOf(g, 'natl'))).toBeGreaterThan(59);
    expect(seaLatitude(g, zoneOf(g, 'natl'))).toBeLessThan(64);
  });

  it('keeps the boats down longer in summer than in winter, the same aircraft over the same sea', () => {
    lay();
    const g = open(), natl = zoneOf(g, 'natl');
    sea(g, 'U-boat Fleet', 'natl', { ss: 40 }, 'raid');
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', natl);
    const at = (month: number): number => { const keep = g.turn; while (turnToDate(g.turn).month !== month) g.turn++; try { return watchAgainst(g, natl, 'GER')!.none; } finally { g.turn = keep; } };
    expect(at(5)).toBeGreaterThan(at(11) * 2);
  });
});
