// The design director's ruling of 15:48Z, 25 November 2026, on the navy's 24-war gate (release 2, the far chart).
// 1. A fleet struck at anchor withdraws beyond where the enemy could strike from next month, at its side's own sight:
//    the enemy's carriers as last reported and a month's steaming on, and the fields of the enemy's strike aircraft.
// 2. The cost of staying counts only the strikes on the harbour it lies in.
// 3. The readiness gather and a strike's way home are made in their month only within a month's steaming; beyond it
//    they are a voyage. A fleet with no harbour it can reach is not scuttled, in passage or in harbour as its base
//    falls: it waits at sea until one opens; the player's waits for his order, and he may scuttle it; with his navy
//    handed over, his admirals make for a harbour as a staff would. (And the rulings of 16:05Z and 16:19Z: what the
//    staff sees, and the enemy's land strike types in service known to every seat at war with it.)
// 4. Once the Pacific lanes are laid a fleet's page lists every lane, nearest first.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { airRange, hasAirfield, km, seaKm } from '../src/campaign/air';
import { fleetFound } from '../src/campaign/carriers';
import { setDesk } from '../src/campaign/depth';
import { SIGHT, sightOf } from '../src/campaign/intel';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import {
  NAVY, adrift, anchorLoss, baseTargets, cameIn, canPass, inPassage, laneSteps, lanesLaid, navalUpkeep, orderedTo, rebase, safeHarbour, seaRoutes, setGate,
  strikeThreat, struckLine, voyageMonths,
} from '../src/campaign/navy';
import { escortLanes } from '../src/campaign/fleets';
import { mineLine, setSeaMines } from '../src/campaign/mines';
import { startOpen } from '../src/campaign/staff';
import { fleetOptions, humanUp, orderRebase, orderScuttle, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, Region } from '../src/campaign/types';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const said = (g: Game, from: number, re: RegExp): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
const gateOf = (g: Game, name: string): number => g.gates.findIndex((x) => (x as { name?: string }).name === name);
const nextMonth = (g: Game, nat = 'JAP'): void => { g.turn++; navalUpkeep(g, nat); };
/** The seas within a month's steaming of a sea, through straits open to a nation, as the threat reckons them. */
function steamed(g: Game, from: number, nat: string): number[] {
  const d = new Map<number, number>([[from, 1]]), q = [from];
  for (let i = 0; i < q.length; i++) {
    if (d.get(q[i])! >= NAVY.range) continue;
    for (const j of g.seas[q[i]].adj) if (!d.has(j) && canPass(g, q[i], j, nat)) { d.set(j, d.get(q[i])! + 1); q.push(j); }
  }
  return q;
}
/** The nearest friendly harbour out of reach of the seas given, in safeHarbour's own order (fewest months, then nearest). */
function nearestBeyond(g: Game, f: Fleet, seas: number[], R: number): Region | null {
  const here = f.base;
  const pick = baseTargets(g, f).filter((r) => seas.every((z) => seaKm(g, r.id, z) > R))
    .map((r) => ({ r, m: voyageMonths(g, f, r), d: km(g, r.id, here) })).sort((a, b) => a.m - b.m || a.d - b.d);
  return pick[0]?.r ?? null;
}

/** The computer's war at its opening: Japan at war with America, America's Pacific Fleet at anchor at Pearl Harbor. */
function atPearl(): { g: Game; f: Fleet; mf: Fleet } {
  const g = startOpen(createGame(3, null));
  if (!atWar(g, 'JAP', 'USA')) declareWar(g, 'JAP', 'USA');
  const f = fleet(g, 'USA', 'Pacific Fleet');
  expect(rebase(g, f, g.byName['Pearl Harbor'], true)).toBe(true);
  return { g, f, mf: fleet(g, 'JAP', 'Mobile Fleet') };
}
const carrierStrike = (g: Game, f: Fleet, sea: number, R: number, by: Fleet): void => {
  f.anchor = [{ t: g.turn, at: f.base, n: 3, lost: 0.2, w: 300, from: [{ sea, field: -1, km: R, nat: by.nat, fleet: by.id }] }];
};

/** A fleet put at sea in a sea of the Indian Ocean, seen by America this month ('exact'), last month only ('report'), or not at all. */
function farOff(g: Game, c: Fleet, seen?: number): number {
  const far = g.seas.findIndex((s) => /Indian/.test(s.name));
  expect(far).toBeGreaterThanOrEqual(0);
  Object.assign(c, { zone: far, stance: 'patrol' });
  delete c.seen;
  if (seen !== undefined) (c.seen ??= {}).USA = seen;
  return far;
}

describe('1. out of reach: where the enemy could strike from next month, at the staff\'s own sight', () => {
  it('the strikers\' carriers, not reported since, are read where the strike showed them, a month\'s steaming on', () => {
    const { g, f, mf } = atPearl();
    farOff(g, mf);
    expect(fleetFound(g, mf, 'USA')).toBe('none');
    const north = g.seas.findIndex((s) => s.name === 'Hawaiian Approaches');
    carrierStrike(g, f, north, 600, mf);
    const r = safeHarbour(g, f)!;
    expect(r).toBeTruthy();
    const month = steamed(g, north, 'JAP');
    expect(month.length).toBeGreaterThan(1);
    for (const z of month) expect(seaKm(g, r.id, z)).toBeGreaterThan(600);
    // Where only the sea of the strike was read, a harbour within a month's steaming of it was taken.
    const old = nearestBeyond(g, f, [north], 600);
    expect(old).toBeTruthy();
    expect(month.some((z) => seaKm(g, old!.id, z) <= 600)).toBe(true);
    expect(r.id).not.toBe(old!.id);
  });

  it('a carrier fleet reported this month, identified or not, is read where it is reported, not where it struck', () => {
    const { g, f, mf } = atPearl();
    const north = g.seas.findIndex((s) => s.name === 'Hawaiian Approaches');
    carrierStrike(g, f, north, 600, mf);
    farOff(g, mf);
    const before = safeHarbour(g, f)!;
    const R = airRange(g.airFleets.find((a) => a.ship === mf.id && a.bmr > 0)!, g, 'harbour').bmr;
    // Identified this month far off in the Indian Ocean, or only reported there: the waters round Hawaii are out of its reach next month.
    for (const seen of [g.turn, g.turn - 1]) {
      const far = farOff(g, mf, seen);
      expect(fleetFound(g, mf, 'USA')).toBe(seen === g.turn ? 'exact' : 'report');
      const T = strikeThreat(g, f);
      expect(T.seas.has(north)).toBe(false);
      expect(T.seas.get(far)).toBe(R);
      const after = safeHarbour(g, f)!;
      expect(after).toBeTruthy();
      expect(after.id).not.toBe(before.id);
      expect(steamed(g, north, 'JAP').some((z) => seaKm(g, after.id, z) <= 600)).toBe(true);
    }
    // Seen two months ago and nothing kept of it since: read where the strike showed it.
    farOff(g, mf, g.turn - 2);
    expect(fleetFound(g, mf, 'USA')).toBe('none');
    expect(safeHarbour(g, f)!.id).toBe(before.id);
    // In its harbour at war it is reported there (the attachés' count): read from its harbour's seas.
    Object.assign(mf, { stance: 'port', zone: mf.home });
    expect(fleetFound(g, mf, 'USA')).not.toBe('none');
    for (const z of g.regions[mf.base].seas) expect(strikeThreat(g, f).seas.get(z)).toBe(R);
  });

  it('the enemy\'s fields: groups where its sight shows them, every field it knows of beyond it with its best type in service', () => {
    const g = startOpen(createGame(3, null));
    declareWar(g, 'ITA', 'UK');
    const f = fleet(g, 'ITA', '1st Fleet'), malta = g.byName['Malta'];
    const sight = sightOf(g, 'ITA');
    // Britain's land strike types in service (and its allies' at war with Italy), known to Italy's staff: the best reach.
    const best = Math.max(...g.airFleets.filter((a) => a.ship === undefined && a.bmr > 0 && atWar(g, a.nat, 'ITA') && (a.nat === 'UK' || allied(g, a.nat, 'UK')))
      .map((a) => airRange(a, g, 'harbour').bmr));
    expect(best).toBeGreaterThan(0);
    const beyond = g.regions.filter((r) => r.ctrl === 'UK' && hasAirfield(g, r.id) && sight[r.id] !== SIGHT.full);
    expect(beyond.length).toBeGreaterThan(5);
    // Struck from a carrier only or from Malta's field, it is the same: every British field beyond its sight counts, with that reach.
    const ark = g.fleets.find((c) => c.nat === 'UK' && c.ships.cv > 0)!;
    for (const from of [{ sea: ark.home, field: -1, km: 300, nat: 'UK', fleet: ark.id }, { sea: -1, field: malta.id, km: 900, nat: 'UK' }]) {
      f.anchor = [{ t: g.turn, at: f.base, n: 2, lost: 0.15, w: 200, from: [from] }];
      const T = strikeThreat(g, f);
      for (const r of beyond) expect(T.fields.get(r.id)).toBeGreaterThanOrEqual(best);
    }
    const first = safeHarbour(g, f);
    if (first) for (const r of beyond) expect(km(g, r.id, first.id)).toBeGreaterThan(best);
    // A field in its sight counts only with a group with strike aircraft on it, with that group's reach.
    const bav = g.byName['Bavaria'];
    bav.ctrl = 'UK';
    expect(sightOf(g, 'ITA')[bav.id]).toBe(SIGHT.full);
    expect(hasAirfield(g, bav.id)).toBe(true);
    expect(strikeThreat(g, f).fields.has(bav.id)).toBe(false);
    const group = g.airFleets.find((a) => a.nat === 'UK' && a.ship === undefined && a.bmr > 0)!;
    group.base = bav.id;
    const R = airRange(group, g, 'harbour').bmr;
    expect(strikeThreat(g, f).fields.get(bav.id)).toBe(R);
    // And a field beyond its sight held by a nation at peace with it counts for nothing.
    const neutral = g.regions.find((r) => sight[r.id] !== SIGHT.full && hasAirfield(g, r.id) && g.nations[r.ctrl] && !atWar(g, r.ctrl, 'ITA') && r.ctrl !== 'ITA' && !allied(g, r.ctrl, 'ITA'))!;
    expect(neutral).toBeTruthy();
    expect(strikeThreat(g, f).fields.has(neutral.id)).toBe(false);
  });
});

describe('2. the cost of staying counts only the strikes on the harbour it lies in', () => {
  it('struck at one harbour and moved to another, it carries none of those losses there', () => {
    const { g, f, mf } = atPearl();
    carrierStrike(g, f, g.seas.findIndex((s) => s.name === 'Hawaiian Approaches'), 600, mf);
    expect(anchorLoss(g, f)).toBeCloseTo(0.2, 9);
    expect(rebase(g, f, g.byName['San Diego'], true)).toBe(true);
    expect(anchorLoss(g, f)).toBe(0);
    expect(safeHarbour(g, f)).toBeNull();
    expect(struckLine(g, f)).toBe('');
  });
});

describe('3. the gather and a strike\'s way home: made in their month only within a month\'s steaming', () => {
  it('beyond it a voyage, from the sea it is on; within it at once; the book\'s other dated moves as they were', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'JAP', 'Mobile Fleet');
    const gather = { base: 'Etorofu', port: 'Hitokappu Bay', home: 'kurile', words: 'The carrier striking force gathers at Hitokappu Bay in the Kuriles' };
    const near = orderedTo(g, f, gather, undefined, true);
    expect(near).toBe(true);
    const within = !inPassage(f);
    // (Hiroshima to the Kuriles: whichever it is, the rule reads the voyage's length.)
    if (within) expect(g.regions[f.base].name).toBe('Etorofu');
    // A strike's way home from far off: Konigsberg, made Japan's, by the Danish straits.
    const kon = g.byName['Konigsberg'];
    kon.ctrl = 'JAP';
    const news = g.headlines.length;
    expect(orderedTo(g, f, { base: 'Konigsberg', port: 'Konigsberg', home: 'baltic', words: 'It comes home' }, undefined, true)).toBe(true);
    expect(inPassage(f)).toBe(true);
    expect(f.base).toBe(kon.id);
    expect(said(g, news, /^The Mobile Fleet sails for its new base at Konigsberg, \d+ seas off, due in /)).toBe(true);
    expect(said(g, news, /^It comes home\.$/)).toBe(false);
    // The book's own dated moves are made in their month, wherever they go.
    const cf = fleet(g, 'JAP', 'Combined Fleet');
    expect(orderedTo(g, cf, { base: 'Konigsberg', port: 'Konigsberg', home: 'baltic', words: 'The book moves it' })).toBe(true);
    expect(cf.stance).toBe('port');
    expect(cf.base).toBe(kon.id);
  });

  it('with no open sea to the harbour there is no move', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'JAP', 'Mobile Fleet'), kon = g.byName['Konigsberg'];
    kon.ctrl = 'JAP';
    setGate(g, gateOf(g, 'the Danish straits'), true);
    const base = f.base;
    expect(orderedTo(g, f, { base: 'Konigsberg', port: 'Konigsberg', home: 'baltic', words: 'It comes home' }, undefined, true)).toBe(false);
    expect(f.base).toBe(base);
  });
});

/** The Combined Fleet in passage from Kure to Konigsberg (made Japan's), its last reach on the Danish straits. */
function lastReach(human: string | null = null): { g: Game; f: Fleet; kon: Region } {
  const g = startOpen(createGame(3, human));
  if (human) for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  const f = fleet(g, 'JAP', 'Combined Fleet'), kon = g.byName['Konigsberg'];
  kon.ctrl = 'JAP';
  expect(rebase(g, f, kon)).toBe(true);
  for (let k = 0; k < 12 && g.turn < f.passage!.due - 1; k++) nextMonth(g);
  return { g, f, kon };
}
/** Every harbour of Japan's side but Konigsberg given to America, and the Danish straits shut: no harbour it can reach. */
function noHarbour(g: Game, kon: Region): void {
  setGate(g, gateOf(g, 'the Danish straits'), true);
  for (const r of g.regions) if (r.port && r !== kon && allied(g, 'JAP', r.ctrl)) r.ctrl = 'USA';
}

describe('3. a fleet in passage with no harbour it can reach is not scuttled', () => {
  it('the computer\'s waits at sea, told once, open to attack; when a way opens it comes in', () => {
    const { g, f, kon } = lastReach();
    noHarbour(g, kon);
    const news = g.headlines.length;
    nextMonth(g);
    expect(g.fleets.includes(f)).toBe(true);
    expect(adrift(f)).toBe(true);
    expect(inPassage(f)).toBe(true);
    const sea = g.seas[f.zone].name;
    expect(said(g, news, /^With its way to Konigsberg shut, and Kure lost, the Combined Fleet has no harbour it can reach: it waits at sea /)).toBe(true);
    expect(said(g, news, /Combined Fleet.*scuttled/)).toBe(false);
    const told = g.headlines.length;
    nextMonth(g);
    expect(adrift(f)).toBe(true);
    expect(g.headlines.length - told).toBe(g.headlines.slice(told).filter((h) => !/Combined Fleet/.test(h.text)).length);
    expect(g.seas[f.zone].name).toBe(sea);
    // Waiting where it is, its panel names a known enemy field on that sea: it is met there each month.
    if (!atWar(g, 'JAP', 'USA')) declareWar(g, 'JAP', 'USA');
    setSeaMines(g, true);
    g.mines = [{ z: f.zone, nat: 'USA', s: 1, t: g.turn, known: ['JAP'] }];
    expect(mineLine(g, f)).not.toBe('');
    setSeaMines(g, false);
    // The straits open again: it makes for its base, and within its reach comes in at once.
    setGate(g, gateOf(g, 'the Danish straits'), false);
    nextMonth(g);
    expect(adrift(f)).toBe(false);
    expect(f.base).toBe(kon.id);
    if (!inPassage(f)) { expect(f.stance).toBe('port'); expect(cameIn(g, f)).toBe(true); }
  });

  it('a fleet in harbour whose base falls with no harbour it can reach puts to sea and waits there, not scuttled', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'GER', 'Swinemunde Fleet'), home = f.home, ships = f.ships.dd + f.ships.ca + f.ships.bb, where = g.regions[f.base].name;
    Object.assign(f, { stance: 'port', zone: home });
    setGate(g, gateOf(g, 'the Danish straits'), true);
    for (const r of g.regions) if (r.port && r.ctrl === 'GER') r.ctrl = 'SOV';
    const news = g.headlines.length;
    navalUpkeep(g, 'GER');
    expect(g.fleets.includes(f)).toBe(true);
    expect(adrift(f)).toBe(true);
    expect(f.zone).toBe(home);
    expect(f.ships.dd + f.ships.ca + f.ships.bb).toBe(ships);
    expect(said(g, news, new RegExp(`^With ${where} lost, the Swinemunde Fleet has no harbour it can reach: it waits at sea in the Baltic Sea, open to attack, until one opens\\.$`))).toBe(true);
    expect(said(g, news, /scuttled/)).toBe(false);
    // A harbour of its own taken back: it makes for it.
    const back = g.regions.find((r) => r.port && r.seas.includes(home) && r.id !== f.base)!;
    back.ctrl = 'GER';
    g.turn++;
    navalUpkeep(g, 'GER');
    expect(adrift(f)).toBe(false);
    expect(f.base).toBe(back.id);
  });

  it('the player\'s waits for his order: a harbour that opens is his to make for, and he may scuttle it', () => {
    const { g, f, kon } = lastReach('JAP');
    noHarbour(g, kon);
    nextMonth(g);
    expect(adrift(f)).toBe(true);
    const o = fleetOptions(g, f.id)!;
    expect(o.scuttle).toBe(true);
    expect(o.ports).toEqual([]);
    expect(o.held).toMatch(/has no harbour it can reach/);
    // The straits open: his fleet does not move by itself.
    setGate(g, gateOf(g, 'the Danish straits'), false);
    nextMonth(g);
    expect(adrift(f)).toBe(true);
    expect(fleetOptions(g, f.id)!.ports).toContain(kon.id);
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    expect(adrift(f)).toBe(false);
    expect(f.base).toBe(kon.id);
    // Not waiting at sea, it cannot be scuttled.
    expect(orderScuttle(g, f.id)).toBe(false);
  });

  it('his navy handed to his admirals, they make for a harbour once one opens, as a staff would; only he scuttles it', () => {
    const { g, f, kon } = lastReach('JAP');
    expect(setDesk(g, 'navy', true)).toBe(true);
    noHarbour(g, kon);
    const news = g.headlines.length;
    nextMonth(g);
    expect(adrift(f)).toBe(true);
    expect(said(g, news, /Its admirals will make for a harbour as soon as one opens; only your order scuttles it\.$/)).toBe(true);
    nextMonth(g);
    expect(g.fleets.includes(f)).toBe(true);
    setGate(g, gateOf(g, 'the Danish straits'), false);
    nextMonth(g);
    expect(adrift(f)).toBe(false);
    expect(f.base).toBe(kon.id);
  });

  it('scuttled on his order, its ships go down with news', () => {
    const { g, f, kon } = lastReach('JAP');
    noHarbour(g, kon);
    nextMonth(g);
    const news = g.headlines.length;
    expect(orderScuttle(g, f.id)).toBe(true);
    expect(g.fleets.includes(f)).toBe(false);
    expect(said(g, news, /^With no harbour it can reach, the Combined Fleet is scuttled at sea .* on your order\.$/)).toBe(true);
  });
});

describe('4. once the Pacific lanes are laid, a fleet\'s page lists every lane, nearest first', () => {
  it('before, only lanes within two months\' steaming; after, all of them, nearest first', () => {
    const g = startOpen(createGame(3, 'UK'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    if (!atWar(g, 'UK', 'GER')) declareWar(g, 'GER', 'UK');
    expect(lanesLaid(g)).toBe(false);
    const all = escortLanes(g, 'UK');
    // A fleet far from some of its country's lanes: the China Fleet, or whichever lists fewer than all.
    const f = g.fleets.find((x) => x.nat === 'UK' && x.ships.dd > 0 && x.stance !== 'passage' && fleetOptions(g, x.id)!.lanes.length < all.length)!;
    expect(f).toBeTruthy();
    const before = fleetOptions(g, f.id)!.lanes.map((l) => l.lane);
    expect(before.length).toBeLessThan(all.length);
    // The Pacific planner's three lanes laid (here, the Hawaii lane's row alone stands for them).
    const routes = seaRoutes(g);
    routes.push({ id: 'america-hawaii', name: 'the Hawaii lane', to: 'USA', zones: [g.seas.findIndex((s) => s.name === 'Hawaiian Waters')], carries: false } as never);
    try {
      expect(lanesLaid(g)).toBe(true);
      const after = fleetOptions(g, f.id)!.lanes;
      expect(after.length).toBe(escortLanes(g, 'UK').length);
      expect(after.length).toBeGreaterThan(before.length);
      const steps = after.map((l) => laneSteps(g, f, escortLanes(g, 'UK').find((x) => x.id === l.lane)!));
      for (let i = 1; i < steps.length; i++) expect(steps[i]).toBeGreaterThanOrEqual(steps[i - 1]);
    } finally { routes.pop(); }
  });
});
