// The navy as pieces on the map, in the grand campaign. A fleet is handled like an army: its ships are laid out as
// cards, and it is given an order. It can be bound to a shipping lane as the escort of its convoys, sent to a sea to
// hunt submarines (which only destroyers and aircraft can sink), stood in to a coast to shell the troops or the
// factories there, split in two and joined to another. The history campaigns keep the war at sea they always had.
import { describe, expect, it } from 'vitest';
import fleets from '../data/campaign/fleets.json';
import { airIncomeLoss } from '../src/campaign/air';
import { createGame, relocate } from '../src/campaign/game';
import { setFog, sightOf, SIGHT } from '../src/campaign/intel';
import {
  FLEETS, NAVY, atSea, escort, escortWorth, fit, fleetReach, huntShareSunk, huntSubs, huntersIn, laneLoss, navalUpkeep, quality, rebase, sail,
  screenGap, seaRoutes, seaWar, shipCount, torpedoRisk, whyNotEscort, zoneOf,
} from '../src/campaign/navy';
import {
  bombard, bombardTargets, detach, escortLanes, fleetCard, gunWeight, merge, planBombard, planEscort, seaOptions, squadronName, whyNotBombard,
  whyNotDetach, whyNotMerge,
} from '../src/campaign/fleets';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  beginHuman, endHuman, fleetOptions, fleetView, humanUp, lanesToGuard, myFleetCards, orderBombard, orderEscort, orderFleet, orderMerge, orderSplit,
  previewBombard, previewEscort, runTurnAI, stepAI,
} from '../src/campaign/turn';
import type { Fleet, Game, Region } from '../src/campaign/types';

const B = FLEETS.bombard;
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const sea = (g: Game, key: string): number => zoneOf(g, key);

/** A grand campaign with Britain at the table, played until Britain is at war and it is her month to give orders. */
function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}

/** An enemy-held coast on a sea, by what is wanted of it. */
function coast(g: Game, zone: number, want: (r: Region) => boolean): Region {
  const r = g.seas[zone].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'GER' && want(x));
  if (!r) throw new Error('no such coast');
  return r;
}

/** Puts a German corps on a coast so that there are troops to shell. */
function garrison(g: Game, r: Region): void {
  const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.kind === 'inf')!;
  relocate(g, c, r);
  c.org = 0.9; c.dug = false;
}

describe('the numbers', () => {
  it('make a bound escort worth more than distant cover, and a hunting group worth more than either', () => {
    expect(fleets.escort.loose).toBeGreaterThan(0);
    expect(fleets.escort.loose).toBeLessThan(0.5);
    expect(fleets.hunt.hunter).toBeGreaterThan(fleets.hunt.escort);
    expect(fleets.hunt.escort).toBeGreaterThan(fleets.hunt.loose);
    expect(fleets.hunt.most).toBeLessThanOrEqual(0.3);
  });

  it('give the guns to battleships and cruisers, and none to carriers and submarines', () => {
    const G = fleets.bombard.guns;
    expect(G.bb).toBeGreaterThan(G.ca);
    expect(G.ca).toBeGreaterThan(G.dd);
    expect(G.cv).toBe(0);
    expect(G.ss).toBe(0);
    // A flotilla of destroyers is no bombardment force: it takes two battleships, or seven cruisers.
    expect(fleets.bombard.least).toBeGreaterThanOrEqual(2 * G.bb);
    expect(fleets.bombard.troops.max).toBeLessThanOrEqual(0.35);
  });

  it('say what every class is for, and name every order', () => {
    for (const c of ['bb', 'cv', 'ca', 'dd', 'ss'] as const) expect(FLEETS.roles[c].length).toBeGreaterThan(20);
    for (const o of ['patrol', 'raid', 'escort', 'hunt', 'port'] as const) expect(FLEETS.orders[o].label.length).toBeGreaterThan(3);
  });
});

describe('escorts on a shipping lane', () => {
  it('guard it in full once bound to it, where a patrol in its seas gives distant cover only', () => {
    const g = britain();
    const wac = named(g, 'Western Approaches Command');
    const lane = seaRoutes(g).find((r) => r.id === 'atlantic')!;
    expect(sail(g, wac, sea(g, 'wapp'), 'patrol')).toBe(true);
    const loose = laneLoss(g, 'UK', lane.zones, 'atlantic');
    expect(escort(g, wac, 'atlantic')).toBe(true);
    expect(wac.stance).toBe('escort');
    expect(wac.lane).toBe('atlantic');
    const bound = laneLoss(g, 'UK', lane.zones, 'atlantic');
    const worth = escortWorth(wac.ships) * quality(g, 'UK') * fit(g, wac);
    expect(bound.guard - loose.guard).toBeCloseTo(worth * (1 - FLEETS.escort.loose), 6);
    expect(bound.raid).toBe(loose.raid);
    if (loose.raid > 0) expect(bound.loss).toBeLessThan(loose.loss);
  });

  it('guard the whole lane, whichever of its seas the fleet is in', () => {
    const g = britain();
    const wac = named(g, 'Western Approaches Command');
    const lane = seaRoutes(g).find((r) => r.id === 'atlantic')!;
    escort(g, wac, 'atlantic');
    const a = laneLoss(g, 'UK', lane.zones, 'atlantic').guard;
    wac.zone = lane.zones.find((z) => z !== wac.zone)!;
    expect(laneLoss(g, 'UK', lane.zones, 'atlantic').guard).toBeCloseTo(a, 9);
    // The convoys sail where the raiders are: the escort goes to the sea of the lane with the most of them.
    escort(g, wac, 'atlantic');
    expect(lane.zones).toContain(wac.zone);
  });

  it('are forecast before the order is given, and the forecast comes true', () => {
    const g = britain();
    const wac = named(g, 'Western Approaches Command');
    orderFleet(g, wac.id, wac.home, 'port');
    const before = { zone: wac.zone, stance: wac.stance, lane: wac.lane };
    const plan = previewEscort(g, wac.id, 'atlantic')!;
    expect(plan.why).toBeNull();
    expect({ zone: wac.zone, stance: wac.stance, lane: wac.lane }).toEqual(before);
    expect(plan.guardThen).toBeGreaterThan(plan.guardNow);
    expect(plan.then).toBeLessThanOrEqual(plan.now);
    expect(orderEscort(g, wac.id, 'atlantic')).toBe(true);
    const lane = seaRoutes(g).find((r) => r.id === 'atlantic')!;
    expect(laneLoss(g, 'UK', lane.zones, 'atlantic').loss).toBeCloseTo(plan.then, 9);
    expect(previewEscort(g, wac.id, 'atlantic')!.bound).toBe(true);
    expect(lanesToGuard(g).find((l) => l.id === 'atlantic')!.escorts).toContain(wac.id);
  });

  it('are refused where they make no sense', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), med = named(g, 'Mediterranean Fleet'), wac = named(g, 'Western Approaches Command');
    // A lane out of reach of the fleet's base.
    expect(whyNotEscort(g, med, 'atlantic')).toMatch(/out of reach/);
    // An enemy's lane.
    expect(whyNotEscort(g, wac, 'german')).toMatch(/no ally/);
    // Battleships alone guard no convoy.
    const guns = detach(g, home, { bb: 2 })!;
    expect(whyNotEscort(g, guns, 'atlantic')).toMatch(/nothing that can guard a convoy/);
    // A lane the blockade has shut carries nothing to guard.
    expect(whyNotEscort(g, named(g, 'Battle Fleet'), 'german')).toMatch(/Nothing sails/);
    expect(escort(g, med, 'atlantic')).toBe(false);
    expect(orderEscort(g, named(g, 'Battle Fleet').id, 'german')).toBe(false);
  });

  it('end when the fleet is given other work', () => {
    const g = britain();
    const wac = named(g, 'Western Approaches Command');
    escort(g, wac, 'atlantic');
    orderFleet(g, wac.id, sea(g, 'wapp'), 'patrol');
    expect(wac.lane).toBeUndefined();
    escort(g, wac, 'atlantic');
    orderFleet(g, wac.id, wac.home, 'port');
    expect(wac.lane).toBeUndefined();
    escort(g, wac, 'atlantic');
    const port = g.regions.find((r) => r.port && r.ctrl === 'UK' && r.id !== wac.base && r.seas.length > 0)!;
    if (rebase(g, wac, port)) expect(wac.lane).toBeUndefined();
  });

  it('are what a computer admiral gives a command tied to a station on its own lane', () => {
    const g = startOpen(createGame(5, null));
    for (let m = 0; m < 6; m++) runTurnAI(g);
    expect(g.nations.UK.atWar.length).toBeGreaterThan(0);
    const wac = named(g, 'Western Approaches Command');
    expect(wac.stance).toBe('escort');
    expect(wac.lane).toBe('atlantic');
    expect(g.convoys.atlantic.guard).toBeGreaterThan(0);
    // Nobody is bound to a lane on which nothing sails.
    for (const f of g.fleets) if (f.stance === 'escort') expect(whyNotEscort(g, f, f.lane!)).toBeNull();
  });
});

describe('destroyers hunt submarines', () => {
  /** German ships in the eastern Mediterranean, where no aircraft of theirs can reach, against British submarines there. */
  function hunt(ships: Partial<Fleet['ships']>, stance: Fleet['stance'] = 'hunt'): { g: Game; hunters: Fleet; quarry: Fleet; z: number } {
    const g = britain();
    const z = sea(g, 'emed');
    const quarry = named(g, 'Mediterranean Fleet');
    quarry.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 60 };
    quarry.zone = z; quarry.stance = 'raid';
    const hunters = named(g, 'Baltic Station');
    hunters.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships };
    hunters.zone = z; hunters.stance = stance; hunters.ready = 1;
    return { g, hunters, quarry, z };
  }

  it('and nothing else does: with no destroyer and no aircraft, no submarine is sunk', () => {
    expect(huntShareSunk(0, 60)).toBe(0);
    const { g, quarry, z } = hunt({ bb: 10, ca: 20 });
    expect(huntersIn(g, 'GER', z).weight).toBe(0);
    for (let m = 0; m < 12; m++) { g.turn++; expect(huntSubs(g, 'GER')[z]).toBeUndefined(); }
    expect(quarry.lost.ss).toBe(0);
  });

  it('by the share the hunt forecasts, and pay for it in destroyers', () => {
    const { g, hunters, quarry, z } = hunt({ dd: 40 });
    const w = huntersIn(g, 'GER', z);
    expect(w.fleets).toContain(hunters);
    expect(w.weight).toBeCloseTo(40 * FLEETS.hunt.hunter * quality(g, 'GER') * fit(g, hunters), 6);
    const share = huntShareSunk(w.weight, 60);
    expect(share).toBeGreaterThan(0.03);
    expect(share).toBeLessThanOrEqual(FLEETS.hunt.most);
    let sunk = 0, expected = 0;
    for (let m = 0; m < 12; m++) {
      g.turn++;
      expected += quarry.ships.ss * huntShareSunk(huntersIn(g, 'GER', z).weight, quarry.ships.ss);
      sunk += huntSubs(g, 'GER')[z] ?? 0;
    }
    expect(sunk).toBe(quarry.lost.ss);
    expect(sunk).toBeGreaterThan(expected * 0.6);
    expect(sunk).toBeLessThan(expected * 1.4);
    expect(hunters.lost.dd).toBeGreaterThan(0);
    expect(g.headlines.some((h) => /sink \d+ British submarines? in the Eastern Mediterranean/.test(h.text))).toBe(true);
  });

  it('hardest when sent out to hunt, less as a lane\'s escort, least on patrol', () => {
    const a = hunt({ dd: 40 }, 'hunt'), b = hunt({ dd: 40 }, 'patrol');
    const hw = huntersIn(a.g, 'GER', a.z).weight, pw = huntersIn(b.g, 'GER', b.z).weight;
    expect(pw).toBeCloseTo(hw * FLEETS.hunt.loose, 6);
    // An escort hunts in every sea of its lane, at the escort's share.
    const g = britain();
    const wac = named(g, 'Western Approaches Command');
    escort(g, wac, 'atlantic');
    const lane = seaRoutes(g).find((r) => r.id === 'atlantic')!;
    for (const z of lane.zones) expect(huntersIn(g, 'UK', z).fleets).toContain(wac);
    expect(huntersIn(g, 'UK', sea(g, 'emed')).fleets).not.toContain(wac);
  });

  it('with carriers beside them, whose aircraft search wide', () => {
    const a = hunt({ dd: 10 }), b = hunt({ dd: 10, cv: 2 });
    expect(huntersIn(b.g, 'GER', b.z).weight).toBeCloseTo(huntersIn(a.g, 'GER', a.z).weight * (10 + 2 * FLEETS.hunt.carrier) / 10, 6);
  });

  it('and screen the heavy ships: a fleet without them is torpedoed', () => {
    const run = (dd: number): { g: Game; home: Fleet } => {
      const g = britain();
      const z = sea(g, 'nsea');
      const subs = named(g, 'U-boat Arm');
      subs.ships.ss = 80; subs.zone = z; subs.stance = 'raid';
      const home = named(g, 'Home Fleet');
      home.ships.dd = dd; home.zone = z; home.stance = 'patrol';
      return { g, home };
    };
    const bare = run(0), screened = run(60);
    expect(screenGap(bare.home)).toBe(1);
    expect(screenGap(screened.home)).toBe(0);
    expect(fleetCard(bare.g, bare.home).screen.word).toMatch(/No destroyers/);
    expect(fleetCard(screened.g, screened.home).screen.word).toBe('Fully screened');
    expect(torpedoRisk(bare.g, bare.home, bare.home.zone)).toBeGreaterThan(0.2);
    expect(torpedoRisk(bare.g, bare.home, bare.home.zone)).toBeLessThanOrEqual(FLEETS.screen.most);
    expect(torpedoRisk(screened.g, screened.home, screened.home.zone)).toBe(0);
    expect(torpedoRisk(bare.g, bare.home, sea(bare.g, 'emed'))).toBe(0);
    const hit = (x: { g: Game }): number => x.g.headlines.filter((h) => /torpedoes and sinks/.test(h.text)).length;
    for (const x of [bare, screened]) for (let m = 0; m < 12; m++) { x.g.turn++; navalUpkeep(x.g, 'UK'); }
    expect(hit(bare)).toBeGreaterThan(0);
    expect(hit(screened)).toBe(0);
  });
});

describe('shelling a coast', () => {
  /** The Home Fleet off the German North Sea coast. */
  function off(): { g: Game; home: Fleet; z: number } {
    const g = britain();
    const home = named(g, 'Home Fleet');
    const z = sea(g, 'nsea');
    expect(orderFleet(g, home.id, z, 'patrol')).toBe(true);
    return { g, home, z };
  }

  it('is refused from harbour, off another sea, on friendly ground, without guns, twice a month, or when worn', () => {
    const { g, home, z } = off();
    const r = coast(g, z, () => true);
    garrison(g, r);
    expect(whyNotBombard(g, home, r, 'troops')).toBeNull();
    const dover = named(g, 'Dover Command');
    sail(g, dover, z, 'patrol');
    expect(whyNotBombard(g, dover, r, 'troops')).toMatch(/too few heavy guns/);
    const med = named(g, 'Mediterranean Fleet');
    sail(g, med, med.home, 'patrol');
    expect(whyNotBombard(g, med, r, 'troops')).toMatch(/is not on the/);
    const own = g.seas[z].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'UK')!;
    expect(whyNotBombard(g, home, own)).toMatch(/not in enemy hands/);
    home.ready = 0.3;
    expect(whyNotBombard(g, home, r, 'troops')).toMatch(/too worn/);
    home.ready = 1;
    sail(g, home, home.home, 'port');
    expect(whyNotBombard(g, home, r, 'troops')).toMatch(/in harbour/);
    sail(g, home, z, 'patrol');
    // A second squadron cannot shell the same coast in the same month, nor the same fleet a second coast.
    const second = detach(g, home, { bb: 3, ca: 4, dd: 8 })!;
    expect(bombard(g, home, r, 'troops')).not.toBeNull();
    expect(whyNotBombard(g, second, r, 'troops')).toMatch(/already been shelled/);
    const other = g.seas[z].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'GER' && x.id !== r.id)!;
    garrison(g, other);
    expect(whyNotBombard(g, home, other, 'troops')).toMatch(/already shelled a coast/);
    expect(whyNotBombard(g, second, other, 'troops')).toBeNull();
  });

  it('cuts the organisation of the troops there by what was forecast, less in shelter, and never below the floor', () => {
    const { g, home, z } = off();
    const r = coast(g, z, () => true);
    garrison(g, r);
    const foes = r.units.filter((u) => u.nat === 'GER');
    foes.forEach((c, i) => { c.org = i === 0 ? 0.9 : 0.35; c.dug = false; });
    const plan = previewBombard(g, home.id, r.id, 'troops')!;
    expect(plan.why).toBeNull();
    expect(plan.guns).toBeCloseTo(gunWeight(g, home), 9);
    expect(plan.cut).toBeGreaterThan(0.05);
    expect(plan.cut).toBeLessThanOrEqual(B.troops.max);
    expect(plan.divs).toBe(foes.reduce((s, c) => s + c.divs, 0));
    const str = foes[0].str, ready = home.ready;
    const rep = orderBombard(g, home.id, r.id, 'troops')!;
    expect(rep.what).toBe('troops');
    expect(rep.cut).toBeGreaterThan(plan.cut * 0.84);
    expect(rep.cut).toBeLessThan(plan.cut * 1.16);
    expect(foes[0].org).toBeCloseTo(Math.max(B.troops.floor, 0.9 - rep.cut), 6);
    expect(foes[0].str).toBeLessThan(str);
    for (const c of foes.slice(1)) expect(c.org).toBeGreaterThanOrEqual(B.troops.floor - 1e-9);
    expect(home.ready).toBeCloseTo(ready - B.ready, 9);
    expect(home.shelled).toBe(g.turn);
    expect(g.shoreReports[g.shoreReports.length - 1]).toBe(rep);
    expect(g.headlines.some((h) => h.text.includes(`shells the German troops at ${r.name}`))).toBe(true);
    // Troops dug in lose less of it.
    const again = off();
    const r2 = again.g.regions[r.id];
    garrison(again.g, r2);
    const dug = r2.units.filter((u) => u.nat === 'GER');
    dug.forEach((c) => { c.org = 0.9; c.dug = true; });
    const rep2 = bombard(again.g, again.home, r2, 'troops')!;
    expect(dug[0].org).toBeCloseTo(0.9 - rep2.cut * B.troops.dug, 6);
  });

  it('stops a share of a factory city\'s output for a month', () => {
    const { g, home, z } = off();
    const r = coast(g, z, (x) => x.fac > 0);
    const plan = previewBombard(g, home.id, r.id, 'city')!;
    expect(plan.why).toBeNull();
    expect(plan.output).toBeGreaterThan(0.1);
    expect(plan.output).toBeLessThanOrEqual(B.city.max);
    expect(plan.nation).toBeGreaterThan(0);
    expect(plan.nation).toBeLessThan(plan.output);
    const when = g.order.indexOf('UK') < g.order.indexOf('GER') ? g.turn : g.turn + 1;
    const was = g.raids.GER?.turn === when ? g.raids.GER.share : 0;
    const rep = orderBombard(g, home.id, r.id, 'city')!;
    expect(rep.output).toBe(plan.output);
    expect(g.raids.GER.turn).toBe(when);
    expect(g.raids.GER.share).toBeGreaterThan(was);
    const keep = g.turn;
    g.turn = when;
    expect(airIncomeLoss(g, 'GER')).toBeGreaterThan(0);
    g.turn = keep;
    // A coast with no industry and no troops in sight is not worth the shells.
    const empty = g.seas[z].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'GER' && x.fac <= 0 && !x.units.length && x.id !== r.id);
    if (empty) expect(whyNotBombard(g, named(g, 'Dover Command'), empty) ?? whyNotBombard(g, detach(g, home, { bb: 2 })!, empty)).not.toBeNull();
  });

  it('is answered by the batteries of a fortified coast, which sink only the light ships that stand in', () => {
    const { g, home, z } = off();
    const r = coast(g, z, () => true);
    garrison(g, r);
    r.fort = { lv: 4, vs: [] };
    const plan = planBombard(g, home, r, 'troops');
    expect(plan.fort).toBe(4);
    expect(plan.reply).toBeCloseTo(B.reply * 4, 9);
    expect(plan.lines.some((l) => l.label === 'Shore batteries')).toBe(true);
    const heavy = home.ships.bb + home.ships.cv;
    const rep = bombard(g, home, r, 'troops')!;
    expect(home.ships.bb + home.ships.cv).toBe(heavy);
    expect(rep.sunk.bb + rep.sunk.cv + rep.sunk.ss).toBe(0);
  });

  it('is aimed through the fog by the fleet itself, which sees the coasts of the sea it is in', () => {
    const g = britain();
    setFog(g, true);
    const home = named(g, 'Home Fleet');
    const z = sea(g, 'nsea');
    for (const f of g.fleets) if (f.nat === 'UK' || f.nat === 'FRA') sail(g, f, f.home, 'port');
    const hidden = g.seas[z].coast.map((id) => g.regions[id]).filter((x) => x.ctrl === 'GER' && sightOf(g, 'UK')[x.id] === SIGHT.none);
    expect(hidden.length).toBeGreaterThan(0);
    const r = hidden[0];
    garrison(g, r);
    // From harbour nothing is known of that coast.
    expect(whyNotBombard(g, home, r, 'troops', 'UK')).toMatch(/in harbour/);
    expect(orderFleet(g, home.id, z, 'patrol')).toBe(true);
    expect(sightOf(g, 'UK')[r.id]).toBe(SIGHT.glimpse);
    expect(whyNotBombard(g, home, r, 'troops', 'UK')).toBeNull();
    expect(bombardTargets(g, home, 'UK').some((x) => x.id === r.id)).toBe(true);
    // A coast on another sea stays dark, and troops there cannot be aimed at.
    const far = g.regions.find((x) => x.ctrl === 'GER' && x.seas.length > 0 && !x.seas.includes(z) && sightOf(g, 'UK')[x.id] === SIGHT.none)!;
    garrison(g, far);
    expect(sightOf(g, 'UK')[far.id]).toBe(SIGHT.none);
    const there = { ...home, zone: far.seas[0] } as Fleet;
    expect(whyNotBombard(g, there, far, 'troops', 'UK')).toMatch(/have been seen/);
    expect(whyNotBombard(g, there, far, 'troops')).toBeNull();
  });

  it('is something the computer\'s admirals do too, beside their armies, and within the same rules', () => {
    let all = 0;
    for (const seed of [1, 2]) {
      const g = startOpen(createGame(seed, null));
      for (let m = 0; m < 14; m++) runTurnAI(g);
      all += g.shoreReports.length;
      for (const rep of g.shoreReports) {
        expect(rep.what).toBe('troops');
        expect(rep.guns).toBeGreaterThanOrEqual(B.least * 0.5);
        expect(rep.cut).toBeLessThanOrEqual(B.troops.max * 1.16);
        expect(g.regions[rep.region].seas).toContain(rep.zone);
      }
      // One coast is shelled once a month at most.
      const keys = g.shoreReports.map((r) => `${r.turn}:${r.region}`);
      expect(new Set(keys).size).toBe(keys.length);
    }
    expect(all).toBeGreaterThan(0);
  }, 30_000);
});

describe('splitting and joining fleets', () => {
  it('splits ships off as a squadron in the same place and under the same order', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    orderFleet(g, home.id, sea(g, 'nsea'), 'patrol');
    const all = shipCount(home.ships);
    expect(whyNotDetach(g, home, {})).toMatch(/Choose/);
    expect(whyNotDetach(g, home, { bb: 99 })).toMatch(/has only/);
    expect(whyNotDetach(g, home, { ...home.ships })).toMatch(/have to stay/);
    const id = orderSplit(g, home.id, { dd: 12 });
    const group = g.fleets.find((f) => f.id === id)!;
    expect(group.name).toBe('1st Escort Group');
    expect(group.ships).toEqual({ bb: 0, cv: 0, ca: 0, dd: 12, ss: 0 });
    expect(shipCount(home.ships) + shipCount(group.ships)).toBe(all);
    expect([group.zone, group.stance, group.base, group.port, group.ready]).toEqual([home.zone, home.stance, home.base, home.port, home.ready]);
    expect(group.tied).toBe(false);
    expect(squadronName(g, 'UK', { bb: 0, cv: 0, ca: 0, dd: 5, ss: 0 })).toBe('2nd Escort Group');
    expect(squadronName(g, 'UK', { bb: 1, cv: 0, ca: 2, dd: 5, ss: 0 })).toBe('1st Battle Squadron');
    expect(squadronName(g, 'UK', { bb: 0, cv: 0, ca: 0, dd: 0, ss: 5 })).toBe('1st Submarine Flotilla');
    // The new group can be given work of its own: this is how destroyers are found for a lane or a hunt.
    expect(orderEscort(g, group.id, 'atlantic')).toBe(fleetReach(g, group).some((z) => seaRoutes(g).find((r) => r.id === 'atlantic')!.zones.includes(z)));
    expect(orderFleet(g, id, sea(g, 'nsea'), 'hunt')).toBe(true);
    expect(group.stance).toBe('hunt');
  });

  it('cannot be used to shell twice in a month', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    const z = sea(g, 'nsea');
    orderFleet(g, home.id, z, 'patrol');
    const r = coast(g, z, () => true);
    garrison(g, r);
    bombard(g, home, r, 'troops');
    const second = detach(g, home, { bb: 3, ca: 4 })!;
    const other = g.seas[z].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'GER' && x.id !== r.id)!;
    garrison(g, other);
    expect(whyNotBombard(g, second, other, 'troops')).toMatch(/already shelled/);
  });

  it('joins two fleets that lie in the same harbour or the same sea', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), med = named(g, 'Mediterranean Fleet');
    expect(whyNotMerge(g, home, med)).toMatch(/not in the same place/);
    expect(whyNotMerge(g, home, named(g, 'Battle Fleet'))).toMatch(/same navy/);
    orderFleet(g, home.id, sea(g, 'nsea'), 'patrol');
    const group = detach(g, home, { ca: 4, dd: 10 })!;
    group.ready = 0.5; home.ready = 1;
    const before = shipCount(home.ships), n = g.fleets.length;
    orderFleet(g, group.id, group.home, 'port');
    expect(orderMerge(g, group.id, home.id)).toBe(false);
    orderFleet(g, group.id, sea(g, 'nsea'), 'patrol');
    expect(fleetOptions(g, group.id)!.merge).toContain(home.id);
    expect(orderMerge(g, group.id, home.id)).toBe(true);
    expect(g.fleets.length).toBe(n - 1);
    expect(g.fleets.includes(group)).toBe(false);
    expect(shipCount(home.ships)).toBe(before + 14);
    expect(home.ready).toBeCloseTo((1 * before + 0.5 * 14) / (before + 14), 9);
    expect(merge(g, home, home)).toBe(false);
  });
});

describe('the cards both interfaces draw', () => {
  it('lay a fleet out by class, heaviest first, and say what it is doing', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    const card = fleetView(g, home.id)!;
    expect(card.cards.map((c) => c.cls)).toEqual(['bb', 'cv', 'ca', 'dd']);
    expect(card.cards[0].count).toBe(home.ships.bb);
    expect(card.cards[0].role).toBe(FLEETS.roles.bb);
    expect(card.admiral!.name).toBe(home.admiral!.name);
    expect(card.guns).toBeCloseTo(gunWeight(g, home), 9);
    orderFleet(g, home.id, home.home, 'port');
    expect(fleetView(g, home.id)!.doing).toBe(`In harbour at ${home.port}`);
    orderFleet(g, home.id, sea(g, 'nsea'), 'patrol');
    expect(fleetView(g, home.id)!.doing).toBe('Patrolling the North Sea');
    orderFleet(g, home.id, sea(g, 'nsea'), 'hunt');
    expect(fleetView(g, home.id)!.doing).toBe('Hunting submarines in the North Sea');
    expect(fleetView(g, home.id)!.order).toBe('Hunt submarines');
    const wac = named(g, 'Western Approaches Command');
    orderEscort(g, wac.id, 'atlantic');
    const w = fleetView(g, wac.id)!;
    expect(w.doing).toBe('Escorting the Atlantic lane');
    expect(w.lane).toBe('atlantic');
    expect(w.screen.gap).toBe(0);
    expect(myFleetCards(g).every((c) => c.nat === 'UK')).toBe(true);
    expect(myFleetCards(g)[0].power).toBeGreaterThanOrEqual(myFleetCards(g)[1].power);
  });

  it('list what a fleet could be told to do, and nothing for a fleet that is not the player\'s', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    orderFleet(g, home.id, sea(g, 'nsea'), 'patrol');
    const o = fleetOptions(g, home.id)!;
    expect(o.seas.map((s) => s.zone)).toEqual(fleetReach(g, home));
    expect(o.seas.find((s) => s.zone === home.zone)!.here).toBe(true);
    expect(o.lanes.map((l) => l.lane)).toContain('atlantic');
    expect(o.split).toBe(true);
    expect(o.coasts).toEqual(bombardTargets(g, home, 'UK').map((r) => r.id));
    expect(o.ports.length).toBeGreaterThan(0);
    expect(fleetOptions(g, named(g, 'Battle Fleet').id)).toBeNull();
    // Where enemy submarines are out, the options say what hunting them there would sink.
    const subs = named(g, 'U-boat Arm');
    subs.zone = sea(g, 'nsea'); subs.stance = 'raid';
    const s = seaOptions(g, home).find((x) => x.zone === subs.zone)!;
    expect(s.subs).toBe(subs.ships.ss);
    expect(s.hunt).toBeGreaterThan(s.huntNow);
    expect([home.zone, home.stance]).toEqual([sea(g, 'nsea'), 'patrol']);
  });

  it('show the lanes a nation has a stake in, with their escorts and their losses', () => {
    const g = britain();
    const lanes = escortLanes(g, 'UK');
    const atl = lanes.find((l) => l.id === 'atlantic')!;
    expect(atl.to).toBe('UK');
    expect(['open', 'raided']).toContain(atl.state);
    expect(lanes.some((l) => l.id === 'german')).toBe(false);
    expect(escortLanes(createGame(3, 'UK'), 'UK')).toEqual([]);
  });

  it('come back from a save as they went in', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), wac = named(g, 'Western Approaches Command');
    const z = sea(g, 'nsea');
    orderFleet(g, home.id, z, 'patrol');
    orderEscort(g, wac.id, 'atlantic');
    const r = coast(g, z, () => true);
    garrison(g, r);
    orderBombard(g, home.id, r.id, 'troops');
    const back = loadGame(saveGame(g))!;
    expect(named(back, 'Western Approaches Command').lane).toBe('atlantic');
    expect(named(back, 'Western Approaches Command').stance).toBe('escort');
    expect(named(back, 'Home Fleet').shelled).toBe(g.turn);
    expect(back.shoreReports).toEqual(g.shoreReports);
    expect(whyNotBombard(back, named(back, 'Home Fleet'), back.regions[r.id], 'troops')).toMatch(/already/);
  });
});

describe('the history campaigns', () => {
  it('keep the war at sea they always had', () => {
    const g = createGame(1939, null);
    expect(seaWar(g)).toBe(false);
    const f = g.fleets.find((x) => x.nat === 'UK' && atSea(x)) ?? named(g, 'Home Fleet');
    expect(sail(g, f, f.home, 'hunt')).toBe(false);
    expect(escort(g, f, 'atlantic')).toBe(false);
    expect(whyNotEscort(g, f, 'atlantic')).toMatch(/grand campaign/);
    expect(whyNotBombard(g, f, g.regions[0])).toMatch(/grand campaign/);
    expect(whyNotDetach(g, f, { dd: 1 })).toMatch(/grand campaign/);
    for (let m = 0; m < 8; m++) runTurnAI(g);
    expect(g.shoreReports).toEqual([]);
    for (const x of g.fleets) {
      expect(['patrol', 'raid', 'port']).toContain(x.stance);
      expect(x.lane).toBeUndefined();
      expect(x.shelled).toBeUndefined();
    }
    expect(g.headlines.some((h) => /torpedoes and sinks|shells the/.test(h.text))).toBe(false);
    // A historical save is written without the grand campaign's shore reports.
    expect(saveGame(g)).not.toContain('shoreReports');
  });

  it('count a patrol in a route\'s seas as its escort, as they always did', () => {
    const g = createGame(1939, null);
    for (let m = 0; m < 3; m++) runTurnAI(g);
    const wac = named(g, 'Western Approaches Command');
    expect(wac.stance).toBe('patrol');
    const lane = NAVY.convoys.find((c) => c.id === 'atlantic')!;
    const zones = lane.zones.map((k) => zoneOf(g, k));
    const withIt = laneLoss(g, 'UK', zones, 'atlantic').guard;
    wac.stance = 'port';
    expect(withIt - laneLoss(g, 'UK', zones, 'atlantic').guard).toBeCloseTo(escortWorth(wac.ships) * quality(g, 'UK') * fit(g, wac), 6);
  });
});
