// The rules added for the war in the Balkans: a side theatre on the mainland with its own dice and its
// own plans, formations only lent to a campaign, airborne troops, havens for troops lifted off a coast,
// and allies who share their shipping.
import { describe, expect, it } from 'vitest';
import {
  addCorps, assess, capture, computeSupply, createGame, declareWar, evacuate, havensOf, refuge, removeCorps, resolveBattle, startPhase, theatreName,
} from '../src/campaign/game';
import { runAI, theatrePhases } from '../src/campaign/ai';
import { NAVY, airborne, landCorps, landingOpts, landingTargets, liftLeft, planLanding, seaOpenAt, whyNotLand, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game, Region } from '../src/campaign/types';

const APRIL_41 = 20, MARCH_41 = 19, JUNE_41 = 22;

const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name);

/** Hands a region to someone else for a test: whoever stood there is taken off the map first. */
function hand(g: Game, r: Region, nat: string): void {
  for (const c of [...r.units]) removeCorps(g, c);
  capture(g, r, nat);
}

/** Greece attacked by Italy, with Britain already in the war against both Axis powers. */
function greekWar(seed = 1): Game {
  const g = createGame(seed, null);
  declareWar(g, 'GER', 'UK');
  declareWar(g, 'ITA', 'UK');
  declareWar(g, 'ITA', 'GRE');
  return g;
}

/** The same, after Germany has come in and taken Athens: Greece has fallen, with British troops still on Crete and in the Peloponnese. */
function greeceFallen(seed = 1): { g: Game; wforce: Corps; creforce: Corps } {
  const g = greekWar(seed);
  declareWar(g, 'GER', 'GRE');
  const creforce = addCorps(g, 'UK', 'Creforce', 'inf', 1, 'Crete', { hold: 1 });
  const wforce = addCorps(g, 'UK', 'W Force', 'inf', 2, 'Peloponnese', { hold: 1 });
  hand(g, g.byName['Thessaly'], 'GER');
  hand(g, g.byName['Athens'], 'GER');
  return { g, wforce, creforce };
}

describe('a theatre on the mainland', () => {
  it('the Balkans are a theatre of their own, with the German assembly areas beside them', () => {
    const g = createGame(1, null);
    const balkans = g.byName['Athens'].theatre;
    expect(balkans).toBeGreaterThanOrEqual(0);
    for (const n of ['Albania', 'Epirus', 'Salonika', 'Crete', 'Belgrade', 'Croatia', 'Macedonia', 'Sofia', 'Plovdiv', 'Banat', 'Transdanubia']) expect(g.byName[n].theatre, n).toBe(balkans);
    for (const n of ['Rome', 'Budapest', 'Bucharest', 'Styria', 'Friuli', 'Berlin', 'London']) expect(g.byName[n].theatre, n).toBe(-1);
    expect(g.byName['Tobruk'].theatre).not.toBe(balkans);
    expect(theatreName(g.byName['Athens'])).toBe('the Balkans');
    expect(theatreName(g.byName['Berlin'])).toBeNull();
    // A saved game is laid over the same map, so the theatre survives a save.
    expect(loadGame(saveGame(g))!.byName['Athens'].theatre).toBe(balkans);
  });

  it('a battle there never touches the dice of the main fronts', () => {
    const g = greekWar();
    const before = g.rng.state;
    const rep = resolveBattle(g, [corps(g, 'Albania Command')!], g.byName['Epirus']);
    expect(rep.defNat).toBe('GRE');
    expect(g.rng.state).toBe(before);
    expect(g.flags.asideBattles).toBe(1);
  });

  it('nor the luck of the generals fighting there', () => {
    // The same battle in Poland, fought with and without a fight in Greece before it, treats its generals alike.
    const poland = (aside: boolean): string => {
      const g = createGame(7, null);
      declareWar(g, 'GER', 'POL');
      if (aside) {
        declareWar(g, 'ITA', 'GRE');
        resolveBattle(g, [corps(g, 'Albania Command')!], g.byName['Epirus']);
      }
      const att = g.corps.find((c) => c.nat === 'GER' && c.general !== null && g.regions[c.reg].nb.some((j) => g.regions[j].ctrl === 'POL' && g.regions[j].units.length > 0))!;
      const target = g.regions[att.reg].nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL' && r.units.length > 0)!;
      const rep = resolveBattle(g, [att], target);
      return JSON.stringify([rep.odds, rep.level, g.generals.filter((x) => x.nat === 'GER' || x.nat === 'POL')]);
    };
    expect(poland(true)).toBe(poland(false));
  });

  it('an army there is told what history did, and a plan that waits on a war says nothing until it comes', () => {
    const g = createGame(1, null);
    g.turn = 14;
    // Italy at peace with Britain has no business in Greece yet.
    expect(theatrePhases(g, 'ITA').find((t) => t.theatre === 'the Balkans')?.phase).toBeNull();
    declareWar(g, 'ITA', 'UK');
    expect(theatrePhases(g, 'ITA').find((t) => t.theatre === 'the Balkans')?.phase?.name).toBe('The invasion of Greece');
    // Germany has no troops there in 1940, and so no plan.
    expect(theatrePhases(g, 'GER').some((t) => t.theatre === 'the Balkans')).toBe(false);
  });

  it('an army there closes up to its front across the theatre\'s own ground, never by a short cut round it', () => {
    const g = greekWar();
    g.turn = 21;
    declareWar(g, 'GER', 'GRE');
    declareWar(g, 'GER', 'YUG');
    // Yugoslavia is overrun, the front stands in Thessaly, and the road round through Romania is open and empty.
    for (const r of g.regions) if (r.owner === 'YUG') hand(g, r, 'GER');
    for (const n of ['Oltenia', 'Sofia', 'Salonika', 'Thessaly']) hand(g, g.byName[n], 'GER');
    expect(g.byName['Oltenia'].theatre).toBe(-1);
    // Both roads south inside the theatre are choked with occupation troops.
    for (const n of ['Serbia', 'Montenegro']) for (let i = 0; i < 5; i++) addCorps(g, 'GER', `Occupation troops ${n} ${i + 1}`, 'inf', 1, n, { hold: 1 });
    const panzers = addCorps(g, 'GER', 'Spare panzer corps', 'arm', 2, 'Belgrade', {});
    panzers.ap = 1; // a tired corps with one march left in it this month
    runAI(g, 'GER');
    // Out through Romania it would have been the main army's to send east the month after.
    expect(g.regions[panzers.reg].theatre).toBe(g.byName['Belgrade'].theatre);
  });
});

describe('formations lent to a campaign', () => {
  const sofia = '5th and 11th Panzer Divisions', plovdiv = '50th, 72nd and 164th Infantry Divisions';

  it('come only once the war they were sent for has begun, and only to a friendly place', () => {
    const g = createGame(1, null);
    declareWar(g, 'GER', 'UK');
    g.turn = MARCH_41;
    startPhase(g, 'GER');
    expect(corps(g, sofia)).toBeUndefined(); // Italy is not at war with Greece
    declareWar(g, 'ITA', 'UK');
    declareWar(g, 'ITA', 'GRE');
    g.turn = MARCH_41 + 1;
    startPhase(g, 'GER');
    expect(corps(g, sofia)).toBeUndefined(); // Bulgaria has not joined the Axis
    g.nations.BUL.align = 'axis';
    g.turn = MARCH_41 + 2;
    startPhase(g, 'GER');
    expect(g.regions[corps(g, sofia)!.reg].name).toBe('Sofia');
    expect(g.regions[corps(g, plovdiv)!.reg].name).toBe('Plovdiv');
    expect(corps(g, sofia)!.hold).toBe(true);
  });

  it('go back to the main army when their time is up, and the rest stay as the garrison', () => {
    const g = greekWar();
    g.nations.BUL.align = 'axis';
    g.turn = APRIL_41;
    startPhase(g, 'GER');
    expect(corps(g, sofia)).toBeDefined();
    g.turn = JUNE_41;
    startPhase(g, 'GER');
    expect(corps(g, sofia)).toBeUndefined();
    expect(corps(g, '2nd and 9th Panzer Divisions')).toBeUndefined();
    expect(corps(g, plovdiv)).toBeDefined();
    expect(g.headlines.some((h) => h.turn === JUNE_41 && /leave for other fronts/.test(h.text) && h.text.includes(sofia))).toBe(true);
  });

  it('are not sent at all once the month they were due back has come', () => {
    const g = greekWar();
    g.nations.BUL.align = 'axis';
    g.turn = JUNE_41;
    startPhase(g, 'GER');
    expect(corps(g, sofia)).toBeUndefined();
    expect(corps(g, plovdiv)).toBeDefined();
  });
});

describe('airborne troops', () => {
  /** German paratroops in a captured Athens, with Crete across the Aegean. */
  function mercury(garrison: boolean): { g: Game; drop: Corps; crete: Region } {
    const { g, creforce } = greeceFallen();
    if (!garrison) removeCorps(g, creforce);
    const drop = addCorps(g, 'GER', 'XI Air Corps', 'inf', 3, 'Athens', { airborne: 1, xp: 0.7 });
    return { g, drop, crete: g.byName['Crete'] };
  }

  it('a force is airborne only if every corps in it is', () => {
    const { g, drop } = mercury(false);
    const foot = addCorps(g, 'GER', 'Infantry', 'inf', 1, 'Athens', {});
    expect(airborne([drop])).toBe(true);
    expect(airborne([drop, foot])).toBe(false);
    expect(airborne([])).toBe(false);
    // The mark survives a save.
    expect(loadGame(saveGame(g))!.corps.find((c) => c.name === 'XI Air Corps')!.airborne).toBe(true);
  });

  it('take an island across one sea without a ship being used', () => {
    const { g, drop, crete } = mercury(false);
    expect(whyNotLand(g, [drop], crete)).toBeNull();
    expect(landingTargets(g, [drop]).map((r) => r.name)).toContain('Crete');
    const ships = liftLeft(g, 'GER');
    const res = landCorps(g, [drop], crete);
    expect(res).toMatchObject({ ok: true, taken: true, air: true, lostAtSea: 0 });
    expect(crete.ctrl).toBe('GER');
    expect(drop.reg).toBe(crete.id);
    expect(liftLeft(g, 'GER')).toBe(ships);
    expect(g.headlines.some((h) => /airborne troops take Crete from the air/.test(h.text))).toBe(true);
  });

  it('reach a coast on the same sea and no further', () => {
    const { g, drop } = mercury(false);
    expect(g.byName['Athens'].seas.some((z) => g.byName['Crete'].seas.includes(z))).toBe(true);
    expect(whyNotLand(g, [drop], g.byName['Malta'])).toMatch(/out of reach/);
    expect(whyNotLand(g, [drop], g.byName['Alexandria'])).toMatch(/out of reach/);
    expect(landingTargets(g, [drop]).map((r) => r.name)).not.toContain('Malta');
  });

  it('can mount one assault a month', () => {
    const { g, drop, crete } = mercury(false);
    const second = addCorps(g, 'GER', '22nd Air Landing Division', 'inf', 1, 'Athens', { airborne: 1 });
    expect(landCorps(g, [drop], crete).ok).toBe(true);
    hand(g, crete, 'UK');
    expect(whyNotLand(g, [second], crete)).toMatch(/one airborne assault a month/);
    g.turn++;
    expect(whyNotLand(g, [second], crete)).toBeNull();
  });

  it('fight without their heavy weapons, and the Battle Ticket says so', () => {
    const { g, drop, crete } = mercury(true);
    const opts = landingOpts(g, [drop], crete);
    expect(opts).toMatchObject({ sea: NAVY.landing.airborne, air: true });
    const a = assess(g, [drop], crete, opts), full = assess(g, [drop], crete);
    expect(a.A / full.A).toBeCloseTo(NAVY.landing.airborne, 6);
    expect(a.lines.some((l) => l.label === 'Airborne assault')).toBe(true);
    expect(a.lines.some((l) => l.label === 'Landing')).toBe(false);
    const plan = planLanding(g, [drop], crete);
    expect(plan).toMatchObject({ why: null, air: true, defended: true, risk: 0 });
    expect(plan.path).toEqual([]);
  });

  it('beaten off, they are back where they took off from', () => {
    const { g, drop, crete } = mercury(true);
    // A garrison far too strong to shift.
    for (let i = 0; i < 3; i++) addCorps(g, 'UK', `Garrison ${i}`, 'inf', 3, 'Crete', {});
    const res = landCorps(g, [drop], crete);
    expect(res).toMatchObject({ ok: true, taken: false, air: true });
    expect(crete.ctrl).toBe('UK');
    expect(g.regions[drop.reg].name).toBe('Athens');
    expect(drop.str).toBeLessThan(1);
    expect(g.headlines.some((h) => /airborne assault on Crete is beaten off/.test(h.text))).toBe(true);
  });

  it('ordinary troops in the same port still need ships and an open sea', () => {
    const { g, crete } = mercury(false);
    const foot = addCorps(g, 'GER', 'Infantry', 'inf', 1, 'Athens', {});
    g.nations.GER.sealift = 0;
    expect(whyNotLand(g, [foot], crete)).toMatch(/no shipping/);
  });
});

describe('lifted off the beaches', () => {
  it('Crete is the haven of the Balkans, and of nowhere else', () => {
    const g = createGame(1, null);
    expect(havensOf(g.byName['Peloponnese'])).toEqual(['Crete']);
    expect(havensOf(g.byName['Tobruk'])).toEqual([]);
    expect(havensOf(g.byName['Calais'])).toEqual([]);
  });

  it('troops taken off a Greek port go to Crete while it is in friendly hands, and home when it is not', () => {
    const { g, wforce } = greeceFallen();
    const london = g.regions[g.nations.UK.capital];
    expect(refuge(g, wforce, g.byName['Peloponnese']).name).toBe('Crete');
    expect(evacuate(g, wforce)).toBe(true);
    expect(g.regions[wforce.reg].name).toBe('Crete');
    expect(wforce.str).toBeCloseTo(0.75, 6);
    expect(g.headlines.some((h) => /lifted off the beaches at Peloponnese and taken to Crete/.test(h.text))).toBe(true);
    // Nobody is lifted off Crete to Crete: from there the way out is home.
    expect(refuge(g, wforce, g.byName['Crete'])).toBe(london);
    // With Crete gone the next lift from the mainland goes home.
    const rearguard = addCorps(g, 'UK', 'Rearguard', 'inf', 1, 'Peloponnese', {});
    hand(g, g.byName['Crete'], 'GER');
    expect(refuge(g, rearguard, g.byName['Peloponnese'])).toBe(london);
  });

  it('beaten with their backs to the sea, they are taken to Crete too', () => {
    const { g, wforce } = greeceFallen();
    const panzers = addCorps(g, 'GER', 'Panzers', 'arm', 3, 'Athens', { xp: 0.5 });
    const more = addCorps(g, 'GER', 'More panzers', 'arm', 3, 'Athens', { xp: 0.5 });
    const rep = resolveBattle(g, [panzers, more], g.byName['Peloponnese']);
    expect(rep.taken).toBe(true);
    expect(rep.defenders[0].fate).toBe('evacuated');
    expect(g.regions[wforce.reg].name).toBe('Crete');
  });

  it('off the theatres, an evacuation still goes home', () => {
    const g = createGame(1, null);
    declareWar(g, 'GER', 'FRA');
    declareWar(g, 'GER', 'UK');
    const bef = addCorps(g, 'UK', 'BEF', 'inf', 2, 'Calais', {});
    expect(evacuate(g, bef)).toBe(true);
    expect(bef.reg).toBe(g.nations.UK.capital);
  });
});

describe('allies share their shipping', () => {
  it('a port in the Balkans that an ally\'s ships can reach feeds the troops who hold it', () => {
    const { g, creforce } = greeceFallen();
    removeCorps(g, creforce);
    const crete = g.byName['Crete'];
    capture(g, crete, 'GER');
    const garrison = addCorps(g, 'GER', 'Fortress Crete', 'inf', 1, 'Crete', {});
    computeSupply(g);
    // Germany has no ships of her own in the Mediterranean: it is the Italian merchant fleet that feeds the island.
    expect(garrison.supplied).toBe(true);
    g.nations.ITA.sealift = 0;
    computeSupply(g);
    expect(garrison.supplied).toBe(false);
  });
});

describe('the expedition to Greece', () => {
  it('stays beside its ally while Athens stands', () => {
    const g = greekWar();
    declareWar(g, 'GER', 'GRE');
    g.turn = APRIL_41;
    const wforce = addCorps(g, 'UK', 'W Force', 'inf', 2, 'Thessaly', { hold: 1 });
    const creforce = addCorps(g, 'UK', 'Creforce', 'inf', 1, 'Crete', { hold: 1 });
    runAI(g, 'UK');
    expect(g.regions[wforce.reg].name).toBe('Thessaly');
    expect(g.regions[creforce.reg].name).toBe('Crete');
  });

  it('is lifted off to Crete once Athens has fallen, and the garrison of Crete stays where it is', () => {
    const { g, wforce, creforce } = greeceFallen();
    g.turn = APRIL_41;
    expect(g.nations.GRE.alive).toBe(false);
    expect(g.byName['Peloponnese'].ctrl).toBe('UK');
    // The Mediterranean Fleet is off Greece to bring them away: without it the German air force closes the Aegean.
    const fleet = g.fleets.find((f) => f.name === 'Mediterranean Fleet')!;
    fleet.zone = fleet.station = zoneOf(g, 'aegean'); fleet.stance = fleet.duty = 'patrol'; fleet.tied = true;
    expect(seaOpenAt(g, 'UK', g.byName['Peloponnese'])).toBe(true);
    runAI(g, 'UK');
    expect(g.regions[wforce.reg].name).toBe('Crete');
    expect(g.regions[creforce.reg].name).toBe('Crete');
    expect(creforce.str).toBe(1);
    // And having reached the haven it does not sail on for home next month.
    g.turn++;
    startPhase(g, 'UK');
    runAI(g, 'UK');
    expect(g.regions[wforce.reg].name).toBe('Crete');
  });
});
