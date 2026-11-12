// The war at sea: the seas, who holds them, fleet actions, convoys, troopships, landings and Norway.
import { describe, expect, it } from 'vitest';
import { addCorps, assess, capture, computeSupply, createGame, declareWar, men } from '../src/campaign/game';
import { income } from '../src/campaign/economy';
import {
  NAVY, airOver, baseTargets, canPass, closedTo, convoyWar, crossingRisk, fleetPower, fleetReach, liftLeft, navalUpkeep, oreRoute,
  sail, seaAt, seaBattles, seaIncomeLoss, shipCount, surfaceShips, zoneOf,
} from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import {
  beginHuman, declarable, humanUp, landTargets, orderDeclare, orderFleet, orderLand, orderRebase, orderShip, passMonth, previewLanding,
  runTurnAI, shipTargets, stepAI,
} from '../src/campaign/turn';
import type { Corps, Fleet, Game } from '../src/campaign/types';

function asPlayer(nat: string, seed = 7): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name)!;
const seasOf = (g: Game, region: string): string[] => g.byName[region].seas.map((z) => g.seas[z].key);

describe('the seas', () => {
  it('every port stands on a sea, and the seas join up both ways', () => {
    const g = createGame(1, null);
    expect(g.seas.length).toBe(20);
    for (const r of g.regions) if (r.port) expect(r.seas.length, r.name).toBeGreaterThan(0);
    for (const z of g.seas) {
      expect(z.coast.length, z.name).toBeGreaterThan(0);
      for (const j of z.adj) expect(g.seas[j].adj, `${z.name} and ${g.seas[j].name}`).toContain(z.id);
    }
    expect(seasOf(g, 'Narvik')).toEqual(['norw']);
    expect(seasOf(g, 'Oslo')).toEqual(['skag']);
    expect(seasOf(g, 'Murmansk')).toContain('barents');
    expect(seasOf(g, 'Leningrad')).toEqual(['finland']);
    expect(seasOf(g, 'Crimea')).toContain('black');
    expect(seasOf(g, 'Kent')).toEqual(['channel']);
    expect(g.byName['Berlin'].seas).toEqual([]);
  });

  it('a point on the chart knows which sea it is in', () => {
    const g = createGame(1, null);
    for (const z of g.seas) expect(seaAt(z.x, z.y), z.name).toBe(z.id);
    expect(seaAt(g.byName['Berlin'].x, g.byName['Berlin'].y)).toBe(-1);
    expect(seaAt(-500, -500)).toBe(-1);
  });

  it('straits belong to whoever holds them', () => {
    const g = createGame(1, null);
    const skag = zoneOf(g, 'skag'), baltic = zoneOf(g, 'baltic');
    // Denmark is neutral: the Danish straits are open to both navies.
    expect(canPass(g, skag, baltic, 'UK')).toBe(true);
    expect(canPass(g, skag, baltic, 'GER')).toBe(true);
    capture(g, g.byName['Zealand'], 'GER');
    expect(canPass(g, skag, baltic, 'UK')).toBe(false);
    expect(canPass(g, skag, baltic, 'GER')).toBe(true);
    // The Turkish straits are shut to every navy at war.
    const aegean = zoneOf(g, 'aegean'), black = zoneOf(g, 'black');
    expect(canPass(g, aegean, black, 'UK')).toBe(false);
    expect(canPass(g, aegean, black, 'SOV')).toBe(true);
    // Gibraltar is British.
    const iberia = zoneOf(g, 'iberia'), wmed = zoneOf(g, 'wmed');
    expect(canPass(g, iberia, wmed, 'ITA')).toBe(true);
    declareWar(g, 'ITA', 'UK');
    expect(canPass(g, iberia, wmed, 'ITA')).toBe(false);
    expect(canPass(g, iberia, wmed, 'FRA')).toBe(true);
  });
});

describe('who holds a sea', () => {
  it('the navies at war are at their stations on the first day, the rest in harbour', () => {
    const g = createGame(1, null);
    const home = fleet(g, 'Home Fleet');
    expect(home.stance).toBe('patrol');
    expect(g.seas[home.zone].key).toBe('nsea');
    expect(fleet(g, 'U-boat Arm').stance).toBe('raid');
    expect(g.seas[fleet(g, 'U-boat Arm').zone].key).toBe('wapp');
    // The German surface fleet waits in port, and Italy is not yet at war.
    expect(fleet(g, 'Battle Fleet').stance).toBe('port');
    expect(fleet(g, '1st Squadron').stance).toBe('port');
    expect(fleetPower(g, home)).toBeGreaterThan(2 * fleetPower(g, fleet(g, 'Battle Fleet')));
  });

  it('aircraft on the coasts close narrow waters, and a strong fleet opens them again', () => {
    const g = createGame(1, null);
    const skag = zoneOf(g, 'skag'), natl = zoneOf(g, 'natl');
    expect(airOver(g, skag, 'UK', true)).toBeGreaterThan(25);
    expect(airOver(g, skag, 'GER', true)).toBe(0);
    expect(closedTo(g, skag, 'UK')).toBe(true);
    expect(closedTo(g, skag, 'GER')).toBe(false);
    // The open ocean is out of reach of all but a few aircraft.
    expect(closedTo(g, natl, 'UK')).toBe(false);
    expect(closedTo(g, natl, 'GER')).toBe(false);
    const home = fleet(g, 'Home Fleet');
    home.zone = skag;
    expect(closedTo(g, skag, 'UK')).toBe(false);
    // Airfields taken this month are not yet in use.
    const before = airOver(g, skag, 'UK', true);
    capture(g, g.byName['Jutland'], 'GER');
    expect(airOver(g, skag, 'UK', true)).toBe(before);
    g.turn++;
    expect(airOver(g, skag, 'UK', true)).toBeGreaterThan(before);
  });

  it('a fleet reaches only the seas near its base', () => {
    const g = createGame(1, null);
    const reach = fleetReach(g, fleet(g, 'Home Fleet')).map((z) => g.seas[z].key);
    expect(reach).toContain('norw');
    expect(reach).toContain('channel');
    expect(reach).not.toContain('wmed');
    expect(reach).not.toContain('black');
    // The Italian fleet cannot pass Gibraltar once it is at war with Britain.
    declareWar(g, 'ITA', 'UK');
    for (const name of ['1st Squadron', '2nd Squadron']) expect(fleetReach(g, fleet(g, name)).map((z) => g.seas[z].key)).not.toContain('iberia');
  });
});

describe('fleet actions', () => {
  const meet = (seed: number): { g: Game; home: Fleet; battle: Fleet } => {
    const g = createGame(seed, null);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    const norw = zoneOf(g, 'norw');
    expect(sail(g, home, norw, 'patrol')).toBe(true);
    expect(sail(g, battle, norw, 'patrol')).toBe(true);
    return { g, home, battle };
  };

  it('the stronger fleet wins, both sides lose ships, and the loser makes for port', () => {
    const { g, home, battle } = meet(5);
    const dice = g.rng.state;
    const reps = seaBattles(g, 'UK');
    expect(reps.length).toBe(1);
    const r = reps[0];
    expect(r.zoneName).toBe('Norwegian Sea');
    expect(r.won).toBe(true);
    expect(shipCount(r.def[0].sunk)).toBeGreaterThan(shipCount(r.att[0].sunk));
    expect(shipCount(battle.ships)).toBe(30 - shipCount(r.def[0].sunk));
    expect(shipCount(battle.lost)).toBe(shipCount(r.def[0].sunk));
    expect(battle.stance).toBe('port');
    expect(home.stance).toBe('patrol');
    expect(battle.ready).toBeLessThan(home.ready);
    expect(g.seaReports).toContain(r);
    expect(r.lines.some((l) => l.label === 'Forces')).toBe(true);
    expect(g.headlines.some((h) => /Home Fleet defeats the Battle Fleet in the Norwegian Sea/.test(h.text))).toBe(true);
    // The sea has its own dice: the land war's are untouched.
    expect(g.rng.state).toBe(dice);
  });

  it('the same seed gives the same battle', () => {
    const a = meet(5), b = meet(5), c = meet(6);
    const ra = seaBattles(a.g, 'UK'), rb = seaBattles(b.g, 'UK'), rc = seaBattles(c.g, 'UK');
    expect(JSON.stringify(rb)).toBe(JSON.stringify(ra));
    expect(rc[0].odds).not.toBe(ra[0].odds);
  });

  it('a weaker fleet does not offer battle, and submarines are no quarry for a battle line', () => {
    const { g } = meet(5);
    expect(seaBattles(g, 'GER')).toEqual([]);
    // The escorts and the U-boats share the Western Approaches without a fleet action.
    const g2 = createGame(5, null);
    expect(seaBattles(g2, 'UK')).toEqual([]);
    expect(surfaceShips(fleet(g2, 'U-boat Arm'))).toBe(0);
  });

  it('admirals learn from battle and can go down with the flagship', () => {
    const { g, home, battle } = meet(5);
    expect(home.admiral!.level).toBe(1);
    // A fleet's strength, ship for ship and with readiness taken out: what is left is the admiral.
    const weight = (f: Fleet): number => Object.entries(f.ships).reduce((s, [c, n]) => s + n * NAVY.classes[c as 'bb'].power, 0);
    const lead = (f: Fleet): number => fleetPower(g, f) / (weight(f) * NAVY.quality[f.nat] * (0.5 + 0.5 * f.ready));
    expect(lead(home)).toBeCloseTo(1, 6);
    seaBattles(g, 'UK');
    g.turn++;
    battle.ships = { bb: 2, cv: 0, ca: 8, dd: 20, ss: 0 };
    battle.ready = 1;
    sail(g, battle, zoneOf(g, 'norw'), 'patrol');
    seaBattles(g, 'UK');
    expect(home.admiral!.level).toBe(2);
    // A level is worth 4% of the fleet's strength.
    expect(lead(home)).toBeCloseTo(1 + NAVY.admiral.perLevel, 6);
    // Across many battles some beaten admiral is lost with his flagship and a new man takes over.
    let lost = 0;
    for (let seed = 1; seed <= 60; seed++) {
      const m = meet(seed);
      seaBattles(m.g, 'UK');
      if (m.battle.admiral!.name !== 'Wilhelm Marschall') {
        lost++;
        expect(m.battle.admiral!.level).toBe(1);
        expect(m.g.headlines.some((h) => h.text === 'Admiral Wilhelm Marschall is lost with his flagship.')).toBe(true);
      }
    }
    expect(lost).toBeGreaterThan(0);
    expect(lost).toBeLessThan(30);
  });

  it('a fleet whose base falls moves to another port, or is scuttled when there is none', () => {
    const g = createGame(1, null);
    const sq = fleet(g, 'Atlantic Squadron');
    const brest = g.byName['Brittany'];
    expect(sq.base).toBe(brest.id);
    capture(g, brest, 'GER');
    navalUpkeep(g, 'FRA');
    expect(sq.base).not.toBe(brest.id);
    expect(g.regions[sq.base].ctrl).toBe('FRA');
    expect(g.regions[sq.base].port).toBeTruthy();
    expect(sq.stance).toBe('port');
    // Poland has one port.
    capture(g, g.byName['Danzig Corridor'], 'GER');
    navalUpkeep(g, 'POL');
    expect(g.fleets.some((f) => f.nat === 'POL')).toBe(false);
  });
});

describe('the convoy war', () => {
  it('raiders cost Britain production, and more when the escorts stay in harbour', () => {
    const g = createGame(3, null);
    convoyWar(g, 'UK');
    const m = g.convoys.atlantic;
    expect(m.tons).toBeGreaterThan(50000);
    expect(m.tons).toBeLessThan(500000);
    expect(seaIncomeLoss(g, 'UK')).toBeCloseTo(m.share, 6);
    expect(m.share).toBeGreaterThan(0.02);
    expect(m.share).toBeLessThan(0.2);

    const bare = createGame(3, null);
    fleet(bare, 'Western Approaches Command').stance = 'port';
    convoyWar(bare, 'UK');
    expect(bare.convoys.atlantic.tons).toBeGreaterThan(m.tons);
    expect(income(bare, 'UK')).toBeLessThan(income(g, 'UK'));

    const quiet = createGame(3, null);
    fleet(quiet, 'U-boat Arm').stance = 'port';
    convoyWar(quiet, 'UK');
    expect(quiet.convoys.atlantic.tons).toBe(0);
    expect(seaIncomeLoss(quiet, 'UK')).toBe(0);
    expect(income(quiet, 'UK')).toBeGreaterThan(income(g, 'UK'));
  });

  it('Swedish ore comes by the Baltic in summer and through Narvik when the Baltic freezes', () => {
    const g = createGame(1, null);
    const narvik = g.byName['Narvik'];
    expect(oreRoute(g)).toBe('baltic'); // September 1939
    g.turn = 5; // January 1940
    expect(oreRoute(g)).toBe('narvik');
    const before = income(g, 'GER');
    declareWar(g, 'GER', 'NOR');
    expect(oreRoute(g)).toBe('cut');
    expect(seaIncomeLoss(g, 'GER')).toBeCloseTo(NAVY.ore.share, 6);
    expect(income(g, 'GER')).toBeLessThan(before);
    capture(g, narvik, 'GER');
    expect(oreRoute(g)).toBe('narvik');
    capture(g, narvik, 'UK');
    expect(oreRoute(g)).toBe('cut');
    g.turn = 9; // May 1940: the ice is gone
    expect(oreRoute(g)).toBe('baltic');
  });
});

describe('armies at sea', () => {
  it('troopships are hunted where the enemy is at sea, and cannot use a sea he commands', () => {
    const g = asPlayer('UK');
    const c = g.byName['Hampshire'].units.find((u) => u.nat === 'UK' && u.kind !== 'fort')!;
    const rouen = g.byName['Rouen'];
    const channel = zoneOf(g, 'channel');
    expect(shipTargets(g, c)).toContain(rouen);
    expect(crossingRisk(g, 'UK', [channel])).toBe(0);

    const battle = fleet(g, 'Battle Fleet');
    expect(sail(g, battle, channel, 'patrol')).toBe(true);
    const risk = crossingRisk(g, 'UK', [channel]);
    expect(risk).toBeGreaterThan(0.1);
    const before = men(c);
    expect(orderShip(g, c, rouen.id)).toBe(true);
    expect(c.reg).toBe(rouen.id);
    expect(c.ap).toBe(0);
    expect(men(c)).toBeLessThan(before);
    expect(men(c)).toBeGreaterThan(before * (1 - NAVY.intercept.loss * risk) - 2);
    expect(liftLeft(g, 'UK')).toBe(g.nations.UK.sealift - 1);

    // With the enemy in overwhelming strength the Channel is shut.
    battle.ships.bb = 40;
    fleet(g, 'Dover Command').stance = 'port';
    expect(closedTo(g, channel, 'UK')).toBe(true);
    const d = g.byName['Kent'].units.find((u) => u.nat === 'UK' && u.kind !== 'fort')!;
    expect(shipTargets(g, d)).not.toContain(rouen);
    expect(orderShip(g, d, rouen.id)).toBe(false);
  });

  it('there is only so much shipping in a month', () => {
    const g = asPlayer('UK');
    const hampshire = g.byName['Hampshire'], rouen = g.byName['Rouen'], cotentin = g.byName['Cotentin'];
    const lift = g.nations.UK.sealift;
    const extra: Corps[] = [];
    for (let i = 0; i <= lift; i++) extra.push(addCorps(g, 'UK', `Test Corps ${i}`, 'inf', 1, i % 2 ? 'Hampshire' : 'Kent', {}));
    for (const c of extra) { c.ap = c.apMax = 4; }
    let shipped = 0;
    for (const c of extra) if (orderShip(g, c, (shipped % 2 ? rouen : cotentin).id)) shipped++;
    expect(shipped).toBe(lift);
    expect(liftLeft(g, 'UK')).toBe(0);
    expect(hampshire.units.concat(g.byName['Kent'].units).filter((c) => extra.includes(c)).length).toBe(1);
  });

  it('a landing in the month war is declared is a surprise; any other fights its way ashore at a discount', () => {
    const g = asPlayer('GER');
    for (let i = 0; i < 7; i++) expect(passMonth(g)).toBe(true);
    expect(g.turn).toBe(8);
    expect(declarable(g)).toContain('NOR');
    expect(orderDeclare(g, 'NOR')).toBe(true);

    const mtn = corps(g, 'Mountain Corps Norway');
    const narvik = g.byName['Narvik'], oslo = g.byName['Oslo'];
    expect(g.regions[mtn.reg].name).toBe('Bremen');
    expect(landTargets(g, [mtn])).toContain(narvik);
    const plan = previewLanding(g, [mtn], narvik.id);
    expect(plan.why).toBeNull();
    expect(plan.surprise).toBe(true);
    expect(plan.risk).toBe(0);
    expect(plan.path.map((z) => g.seas[z].key)).toEqual(['nsea', 'norw']);
    const strength = men(mtn);
    const res = orderLand(g, [mtn], narvik.id);
    expect(res.ok).toBe(true);
    expect(res.taken).toBe(true);
    expect(narvik.ctrl).toBe('GER');
    expect(mtn.reg).toBe(narvik.id);
    expect(mtn.ap).toBe(0);
    expect(men(mtn)).toBe(strength);
    expect(liftLeft(g, 'GER')).toBe(g.nations.GER.sealift - 1);

    // Oslo is defended. On the day of the declaration the ticket says so, and the attack is at full strength.
    const xxi = corps(g, 'Group XXI');
    const surprise = previewLanding(g, [xxi], oslo.id);
    expect(surprise.defended).toBe(true);
    expect(surprise.assess!.lines.find((l) => l.label === 'Landing')!.value).toMatch(/day war is declared/);
    // A month later the same landing fights at 60%.
    g.flags['war:GER:NOR'] = g.turn - 1;
    const late = previewLanding(g, [xxi], oslo.id);
    expect(late.surprise).toBe(false);
    expect(late.assess!.lines.find((l) => l.label === 'Landing')!.effect).toMatch(/60%/);
    expect(late.assess!.A).toBeCloseTo(surprise.assess!.A * NAVY.landing.assault, 3);
    expect(assess(g, [xxi], oslo, { sea: NAVY.landing.assault }).A).toBeCloseTo(late.assess!.A, 6);

    // A corps that is not in a port, or a coast that is not the enemy's, is no landing.
    expect(previewLanding(g, [xxi], g.byName['Stockholm'].id).why).toMatch(/not enemy ground/);
    const inland = g.corps.find((c) => c.nat === 'GER' && c.kind !== 'fort' && !g.regions[c.reg].port)!;
    expect(orderLand(g, [inland], oslo.id).ok).toBe(false);
  });

  it('troops landed across a sea the enemy holds are cut off until their own navy opens it', () => {
    const g = createGame(2, null);
    declareWar(g, 'GER', 'NOR');
    const narvik = g.byName['Narvik'];
    const c = addCorps(g, 'GER', 'Test Mountain Corps', 'mtn', 1, 'Narvik', {});
    capture(g, narvik, 'GER');
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    const norw = zoneOf(g, 'norw');
    sail(g, home, norw, 'patrol');
    computeSupply(g);
    expect(closedTo(g, norw, 'GER')).toBe(true);
    expect(c.supplied).toBe(false);
    sail(g, home, home.home, 'port');
    sail(g, battle, norw, 'patrol');
    expect(g.supplyDirty).toBe(true);
    computeSupply(g);
    expect(closedTo(g, norw, 'GER')).toBe(false);
    expect(c.supplied).toBe(true);
  });
});

describe('the player and the navy', () => {
  it('orders reach only the player\'s own fleets, within reach of their bases', () => {
    const g = asPlayer('UK');
    const home = fleet(g, 'Home Fleet');
    expect(orderFleet(g, home.id, zoneOf(g, 'norw'), 'patrol')).toBe(true);
    expect(g.seas[home.zone].key).toBe('norw');
    expect(orderFleet(g, home.id, zoneOf(g, 'black'), 'patrol')).toBe(false);
    expect(orderFleet(g, fleet(g, 'Battle Fleet').id, zoneOf(g, 'nsea'), 'patrol')).toBe(false);
    expect(orderFleet(g, home.id, home.home, 'port')).toBe(true);
    expect(home.stance).toBe('port');

    const kent = g.byName['Kent'];
    expect(baseTargets(g, home)).toContain(kent);
    expect(baseTargets(g, home)).not.toContain(g.byName['Leningrad']);
    expect(orderRebase(g, home.id, g.byName['Leningrad'].id)).toBe(false);
    expect(orderRebase(g, home.id, kent.id)).toBe(true);
    expect(home.port).toBe('Kent');
    expect(g.seas[home.home].key).toBe('channel');
    expect(home.stance).toBe('port');
  });

  it('a nation with troopships can declare war across the water, one without cannot', () => {
    expect(declarable(asPlayer('GER'))).toContain('NOR');
    expect(declarable(asPlayer('POL'))).not.toContain('NOR');
  });

  it('a saved game keeps its fleets, convoys and sea battles, and plays on the same', () => {
    const a = createGame(21, null);
    for (let t = 1; t <= 9; t++) runTurnAI(a);
    expect(a.seaReports.length).toBeGreaterThan(0);
    const b = loadGame(saveGame(a))!;
    expect(b.fleets).toEqual(a.fleets);
    expect(b.convoys).toEqual(a.convoys);
    expect(b.seaReports).toEqual(a.seaReports.slice(-80));
    expect(b.nextFleet).toBe(a.nextFleet);
    for (let t = 0; t < 3; t++) { runTurnAI(a); runTurnAI(b); }
    expect(saveGame(b)).toBe(saveGame(a));
  });
});

describe('Norway, April 1940', () => {
  for (const seed of [1, 7]) {
    it(`the Germans land, the navies meet, and the north holds until France falls (seed ${seed})`, () => {
      const g = createGame(seed, null);
      const battle = fleet(g, 'Battle Fleet');
      // The German surface fleet stays in port through the winter.
      while (g.turn < 8) { runTurnAI(g); if (g.turn < 8) expect(battle.stance).toBe('port'); }
      runTurnAI(g);
      for (const name of ['Oslo', 'Narvik', 'Trondheim', 'Bergen']) expect(g.byName[name].ctrl, name).toBe('GER');
      expect(g.nations.DEN.alive).toBe(false);
      const action = g.seaReports.find((r) => r.turn === 8 && r.zoneName === 'Norwegian Sea');
      expect(action).toBeDefined();
      expect(action!.attNat).toBe('UK');
      expect(action!.won).toBe(true);
      expect(g.headlines.some((h) => h.turn === 8 && /German troops land at Narvik from the sea/.test(h.text))).toBe(true);
      // The British expedition is ashore in the north.
      const exp = corps(g, 'North Western Expeditionary Force');
      expect(exp).toBeDefined();
      expect(g.regions[exp.reg].owner).toBe('NOR');
      expect(g.nations.NOR.alive).toBe(true);

      while (g.turn <= 12) runTurnAI(g);
      expect(g.nations.NOR.alive).toBe(false);
      expect(g.nations.NOR.fellOn!.turn).toBeGreaterThanOrEqual(9);
      expect(g.nations.NOR.fellOn!.turn).toBeLessThanOrEqual(12);
      expect(g.nations.FRA.alive).toBe(false);
      // No British soldier is left in Norway.
      expect(g.corps.some((c) => c.nat === 'UK' && g.regions[c.reg].owner === 'NOR')).toBe(false);
      // The convoy war has gone on all the while.
      expect(g.convoys.atlantic.total).toBeGreaterThan(1000000);
      expect(shipCount(fleet(g, 'U-boat Arm').lost)).toBeGreaterThan(0);
    });
  }
});
