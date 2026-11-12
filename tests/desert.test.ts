// The rules that make the war overseas different. The colonies do not decide a nation's fate,
// the desert has one road and no water, and an army there is only as strong as the port behind it.
import { describe, expect, it } from 'vitest';
import {
  DESERT, addCorps, armySize, assess, capitulate, capture, collapseScore, computeSupply, convoyDrag, createGame, declareWar,
  fieldArmy, inDesert, lineDrag, lineStrength, moveCost, railMove, railReach, removeCorps, resolveBattle, startPhase,
} from '../src/campaign/game';
import { theatrePhases } from '../src/campaign/ai';
import { canRaise, orderCorps } from '../src/campaign/economy';
import { closedTo, convoyWar, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game, Region } from '../src/campaign/types';

/** September 1939 with Italy already in the war, which is what most of these rules need. */
function desertWar(seed = 1): Game {
  const g = createGame(seed, null);
  declareWar(g, 'ITA', 'UK');
  return g;
}

const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name)!;

/** Hands a region to someone else for a test: whoever stood there is taken off the map first. */
function hand(g: Game, r: Region, nat: string): void {
  for (const c of [...r.units]) removeCorps(g, c);
  capture(g, r, nat);
}

/** A month of convoy figures with this share of the cargo sunk. */
const sunk = (g: Game, loss: number) => ({ turn: g.turn, raid: 0, guard: 0, tons: 0, total: 0, share: 0, loss, subsLost: 0, escortsLost: 0 });

describe('the colonies', () => {
  it('North Africa, Malta and Gibraltar are overseas; the home countries are not', () => {
    const g = createGame(1, null);
    for (const n of ['Tripoli', 'Benghazi', 'Tobruk', 'Alexandria', 'Cairo', 'Malta', 'Gibraltar', 'Algiers', 'Tunis', 'Rhodes']) expect(g.byName[n].overseas, n).toBe(true);
    for (const id of ['ITA', 'UK', 'FRA', 'GER']) expect(g.regions[g.nations[id].capital].overseas, id).toBe(false);
    expect(g.byName['Sicily'].overseas).toBe(false);
    // A saved game is laid over the same map, so the flag survives a save.
    expect(loadGame(saveGame(g))!.byName['Tripoli'].overseas).toBe(true);
  });

  it('troops overseas are not counted in the army that defends the homeland', () => {
    const g = createGame(1, null);
    for (const id of ['ITA', 'UK', 'FRA']) {
      const away = g.corps.filter((c) => c.nat === id && g.regions[c.reg].overseas).reduce((s, c) => s + c.divs * c.str, 0);
      expect(away, id).toBeGreaterThan(0);
      expect(fieldArmy(g, id) - armySize(g, id), id).toBeCloseTo(away, 6);
    }
    // Germany starts with nobody overseas.
    expect(fieldArmy(g, 'GER')).toBe(armySize(g, 'GER'));
  });

  it('losing every colony does not bring a nation nearer to collapse', () => {
    const g = desertWar();
    const before = collapseScore(g, 'ITA');
    for (const r of g.regions) if (r.owner === 'ITA' && r.overseas) capture(g, r, 'UK');
    expect(g.byName['Tripoli'].ctrl).toBe('UK');
    expect(collapseScore(g, 'ITA')).toBe(before);
    expect(g.nations.ITA.alive).toBe(true);
  });

  it('new corps are raised at home, never in a colony', () => {
    const g = createGame(1, null);
    g.nations.UK.pp = 999;
    expect(canRaise(g, 'UK', g.byName['London'])).toBe(true);
    expect(canRaise(g, 'UK', g.byName['Cairo'])).toBe(false);
    expect(orderCorps(g, 'UK', 'inf', g.byName['Cairo'].id)).toBeNull();
    expect(orderCorps(g, 'UK', 'inf', g.byName['London'].id)).not.toBeNull();
  });

  it('an inland depot is a base, not a port', () => {
    const g = createGame(1, null);
    expect(g.byName['Cairo'].port).toBe(false);
    expect(g.byName['Cairo'].seas.length).toBe(0);
    expect(g.byName['Alexandria'].port).toBe(true);
    computeSupply(g);
    expect(g.reach.UK[g.byName['Cairo'].id]).toBe(0);
  });

  it('when France falls the Army of Africa stays at its posts under Vichy', () => {
    const g = createGame(1, null);
    capitulate(g, 'FRA', 'GER');
    for (const name of ['Army of Africa', 'Tunisia Command', 'Morocco Command']) {
      const c = corps(g, name);
      expect(c, name).toBeDefined();
      expect(c.nat, name).toBe('VIC');
      expect(c.hold, name).toBe(true);
      expect(g.regions[c.reg].ctrl, name).toBe('VIC');
    }
    expect(g.corps.some((c) => c.nat === 'FRA')).toBe(false);
  });
});

describe('dice overseas', () => {
  const fight = (g: Game) => {
    addCorps(g, 'UK', 'Frontier Force', 'inf', 1, 'Sidi Barrani', {});
    return resolveBattle(g, [corps(g, 'XXI Corps, 10th Army'), corps(g, 'XXIII Corps, 10th Army')], g.byName['Sidi Barrani']);
  };

  it('a battle in the desert never touches the dice of the war in Europe', () => {
    const quiet = desertWar(7), busy = desertWar(7);
    fight(busy);
    expect(busy.reports.length).toBe(1);
    expect(busy.rng.state).toBe(quiet.rng.state);
  });

  it('the same battle with the same seed comes out the same way', () => {
    const a = fight(desertWar(7)), b = fight(desertWar(7));
    expect(a.odds).toBe(b.odds);
    expect(a.level).toBe(b.level);
    // A different seed is a different roll.
    expect(fight(desertWar(8)).odds).not.toBe(a.odds);
  });
});

describe('the supply line in the desert', () => {
  it('is counted from the nearest working port, a step for every region of desert', () => {
    const g = createGame(1, null);
    computeSupply(g);
    const ita = (n: string): number => g.reach.ITA[g.byName[n].id], uk = (n: string): number => g.reach.UK[g.byName[n].id];
    // Tripoli is a great harbour. Benghazi and Tobruk are small ones and start as if two regions out.
    expect(ita('Tripoli')).toBe(0);
    expect([ita('Misrata'), ita('Sirte'), ita('El Agheila')]).toEqual([1, 2, 3]);
    expect(ita('Benghazi')).toBe(DESERT.ports.Benghazi);
    expect(ita('Tobruk')).toBe(DESERT.ports.Tobruk);
    expect(ita('Bardia')).toBe(DESERT.ports.Tobruk + DESERT.step);
    // The British are fed from the delta.
    expect([uk('Alexandria'), uk('El Alamein'), uk('Mersa Matruh'), uk('Sidi Barrani')]).toEqual([0, 1, 2, 3]);
  });

  it('thins an army out the further it stands from its port', () => {
    const g = createGame(1, null);
    computeSupply(g);
    // No general, so the plain scale applies.
    const c = corps(g, 'XXIII Corps, 10th Army');
    expect(c.general).toBeNull();
    const at = (place: string) => { removeCorps(g, c); const d = addCorps(g, 'ITA', c.name, 'inf', 2, place, {}); computeSupply(g); return lineStrength(g, d); };
    expect(at('Tripoli')).toEqual({ mul: 1, length: 0 });
    expect(at('Tobruk').mul).toBe(1);
    expect(at('Bardia')).toEqual({ mul: 0.95, length: 3 });
    // The scale only ever goes down.
    for (let i = 1; i < DESERT.bands.length; i++) expect(DESERT.bands[i][1]).toBeLessThan(DESERT.bands[i - 1][1]);
    // The home country is no desert: the same corps in Italy is at full strength.
    expect(inDesert(g.regions[g.nations.ITA.capital])).toBe(false);
  });

  it('a captured port shortens the line of the army that took it', () => {
    const g = desertWar();
    hand(g, g.byName['Bardia'], 'UK');
    computeSupply(g);
    expect(g.reach.UK[g.byName['Bardia'].id]).toBe(4);
    hand(g, g.byName['Tobruk'], 'UK');
    computeSupply(g);
    expect(g.reach.UK[g.byName['Tobruk'].id]).toBe(DESERT.ports.Tobruk);
    expect(g.reach.UK[g.byName['Bardia'].id]).toBe(DESERT.ports.Tobruk + DESERT.step);
  });

  it('grows for the Axis with every tenth of the Libya convoys sunk, and not for the British', () => {
    const g = desertWar();
    expect(convoyDrag(g, 'ITA')).toBe(0);
    g.convoys[DESERT.route.id] = sunk(g, 0.2);
    expect(convoyDrag(g, 'ITA')).toBeCloseTo(2 * DESERT.route.perTenth, 6);
    expect(convoyDrag(g, 'GER')).toBeCloseTo(2 * DESERT.route.perTenth, 6);
    expect(convoyDrag(g, 'UK')).toBe(0);
    computeSupply(g);
    expect(g.reach.ITA[g.byName['Tripoli'].id]).toBeCloseTo(1, 6);
    expect(g.reach.ITA[g.byName['Bardia'].id]).toBeCloseTo(4, 6);
    expect(g.reach.UK[g.byName['Mersa Matruh'].id]).toBe(2);
    // Old figures stop counting: two months on with no new report, the line is back to its length.
    g.turn += 2;
    expect(convoyDrag(g, 'ITA')).toBe(0);
  });

  it('an ally\'s working port feeds the corps who came to fight beside him', () => {
    const g = desertWar();
    const dak = addCorps(g, 'GER', '5th Light Division', 'arm', 1, 'Sirte', {});
    computeSupply(g);
    expect(dak.supplied).toBe(true);
    expect(lineStrength(g, dak).length).toBe(2);
    // With every Libyan port in British hands there is nothing to live on.
    for (const n of ['Tripoli', 'Benghazi', 'Tobruk']) hand(g, g.byName[n], 'UK');
    computeSupply(g);
    expect(dak.supplied).toBe(false);
  });

  it('a fortified port keeps stores: it feeds its garrison for a while after the sea is closed', () => {
    const g = desertWar();
    const tobruk = g.byName['Tobruk'];
    hand(g, tobruk, 'UK');
    expect(tobruk.fort).not.toBeNull();
    const garrison = addCorps(g, 'UK', 'Tobruk garrison', 'inf', 3, 'Tobruk', {});
    computeSupply(g);
    expect(garrison.supplied).toBe(true);
    // The Italian battle fleet closes the Libyan Sea. Tobruk is cut off by land and by water.
    const libyan = zoneOf(g, 'libyan');
    for (const f of g.fleets) if (f.nat === 'ITA') { f.zone = libyan; f.stance = 'patrol'; }
    expect(closedTo(g, libyan, 'UK')).toBe(true);
    for (let month = 1; month <= DESERT.stores; month++) {
      g.turn++;
      computeSupply(g);
      expect(garrison.supplied, `month ${month}`).toBe(true);
    }
    g.turn++;
    computeSupply(g);
    expect(garrison.supplied).toBe(false);
    // An open port with no works has no such reserve.
    const h = desertWar();
    hand(h, h.byName['Benghazi'], 'UK');
    const exposed = addCorps(h, 'UK', 'Benghazi force', 'inf', 1, 'Benghazi', {});
    for (const f of h.fleets) if (f.nat === 'ITA') { f.zone = zoneOf(h, 'libyan'); f.stance = 'patrol'; }
    computeSupply(h);
    expect(exposed.supplied).toBe(false);
  });

  it('slows an advance into enemy desert the longer the line behind it', () => {
    const g = desertWar();
    const c = corps(g, 'XXIII Corps, 10th Army');
    const sidi = g.byName['Sidi Barrani'], tobruk = g.byName['Tobruk'];
    const push = (loss: number): number => { g.convoys[DESERT.route.id] = sunk(g, loss); computeSupply(g); return lineDrag(g, c, sidi); };
    // From Bardia the line is three regions long; the next bound makes four.
    expect(push(0)).toBe(0);
    expect(push(0.4)).toBe(1);
    expect(push(0.8)).toBe(2);
    expect(moveCost(g, c, sidi)).toBeGreaterThan(moveCost(g, c, tobruk));
    // Marching back through desert already held costs nothing extra, however long the line.
    expect(lineDrag(g, c, tobruk)).toBe(0);
  });
});

describe('railways', () => {
  it('none crosses the desert: troops there march or go by sea', () => {
    const g = createGame(1, null);
    const name = (ids: Set<number>): string[] => [...ids].map((id) => g.regions[id].name);
    // The delta has track. The desert west of it has none.
    const delta = name(railReach(g, corps(g, 'Alexandria garrison')));
    expect(delta).toContain('Cairo');
    expect(delta).toContain('Port Said');
    expect(delta).not.toContain('El Alamein');
    expect(delta).not.toContain('Mersa Matruh');
    expect(railReach(g, corps(g, '7th Armoured Division')).size).toBe(0);
    expect(railMove(g, corps(g, '7th Armoured Division'), g.byName['Alexandria'])).toBe(false);
    // Tripoli and Benghazi are joined only by the coast road.
    const libya = addCorps(g, 'ITA', 'Tripoli garrison', 'inf', 1, 'Tripoli', {});
    libya.ap = 4;
    expect(railMove(g, libya, g.byName['Benghazi'])).toBe(false);
    // Europe is unchanged: a corps in Italy can still go anywhere the track runs.
    const po = corps(g, 'Army of the Po');
    po.ap = 4;
    expect(railMove(g, po, g.regions[g.nations.ITA.capital])).toBe(true);
  });
});

describe('open desert', () => {
  /** Italian infantry dug in nowhere, at Sidi Barrani, with the Western Desert Force in front of it. */
  function frontier(seed = 1): { g: Game; armour: Corps; foot: Corps; target: Region } {
    const g = desertWar(seed);
    const target = g.byName['Sidi Barrani'];
    capture(g, target, 'ITA');
    addCorps(g, 'ITA', 'Sidi Barrani camps', 'inf', 2, 'Sidi Barrani', {});
    return { g, armour: corps(g, '7th Armoured Division'), foot: corps(g, '4th Indian Division'), target };
  }

  it('foot infantry outside prepared works is driven round by tanks and lorried troops', () => {
    const { g, armour, foot, target } = frontier();
    const tanks = assess(g, [armour], target), marching = assess(g, [foot], target);
    expect(tanks.lines.some((l) => l.label === 'Open desert')).toBe(true);
    expect(marching.lines.some((l) => l.label === 'Open desert')).toBe(false);
    // Same defenders, same ground: against a mobile attack they are worth 'open' of what they are against infantry.
    expect(tanks.D / marching.D).toBeCloseTo(DESERT.open, 6);
  });

  it('does not apply inside a fortress, or to troops with their own wheels', () => {
    const { g, armour, target } = frontier();
    expect(assess(g, [armour], g.byName['Tobruk']).lines.some((l) => l.label === 'Open desert')).toBe(false);
    for (const c of [...target.units]) removeCorps(g, c);
    addCorps(g, 'ITA', 'Lorried column', 'mot', 1, 'Sidi Barrani', {});
    expect(assess(g, [armour], target).lines.some((l) => l.label === 'Open desert')).toBe(false);
  });

  it('men on foot driven out by a mobile enemy lose more than the troops who can drive away', () => {
    const { g, armour, target } = frontier();
    const lorried = addCorps(g, 'ITA', 'Lorried column', 'mot', 2, 'Sidi Barrani', {});
    const walking = corps(g, 'Sidi Barrani camps');
    const more = addCorps(g, 'UK', '2nd Armoured Division', 'arm', 2, 'Mersa Matruh', {});
    const rep = resolveBattle(g, [armour, more], target);
    expect(rep.taken).toBe(true);
    const ratio = (1 - walking.str) / (1 - lorried.str);
    expect(ratio).toBeCloseTo(DESERT.retreat[rep.odds >= 2 ? 1 : 0], 6);
    expect(DESERT.retreat[1]).toBeGreaterThan(DESERT.retreat[0]);
  });
});

describe('formations sent overseas', () => {
  const name = '6th Australian Division';
  const SEPT_40 = 13;

  it('sail only once the war they were sent for has begun', () => {
    const g = createGame(1, null);
    g.turn = SEPT_40;
    startPhase(g, 'UK');
    expect(corps(g, name)).toBeUndefined();
    declareWar(g, 'ITA', 'UK');
    g.turn++;
    startPhase(g, 'UK');
    expect(g.regions[corps(g, name).reg].name).toBe('Alexandria');
    // Once only.
    g.turn++;
    startPhase(g, 'UK');
    expect(g.corps.filter((c) => c.name === name).length).toBe(1);
  });

  it('wait for a friendly port, and are cancelled after half a year without one', () => {
    const g = desertWar();
    g.turn = SEPT_40;
    hand(g, g.byName['Alexandria'], 'ITA');
    startPhase(g, 'UK');
    expect(corps(g, name)).toBeUndefined();
    // The port is won back: the division lands a month late.
    hand(g, g.byName['Alexandria'], 'UK');
    g.turn++;
    startPhase(g, 'UK');
    expect(corps(g, name)).toBeDefined();

    const late = desertWar();
    late.turn = SEPT_40;
    hand(late, late.byName['Alexandria'], 'ITA');
    for (let i = 0; i < 7; i++) { startPhase(late, 'UK'); late.turn++; }
    hand(late, late.byName['Alexandria'], 'UK');
    startPhase(late, 'UK');
    expect(corps(late, name)).toBeUndefined();
  });

  it('the garrison meant for Tobruk does not land in a Tobruk the enemy holds', () => {
    const g = desertWar();
    g.turn = 18;
    startPhase(g, 'UK');
    expect(g.corps.some((c) => c.nat === 'UK' && g.regions[c.reg].name === 'Tobruk')).toBe(false);
  });
});

describe('a plan of its own', () => {
  it('an army overseas is told what history did there, month by month', () => {
    const g = desertWar();
    g.turn = 13;
    const [ita] = theatrePhases(g, 'ITA');
    expect(ita.theatre).toBe('North Africa');
    expect(ita.phase).toMatchObject({ name: 'The invasion of Egypt', posture: 'attack', objectives: ['Sidi Barrani'] });
    // The British wait: no local counter-attacks until Operation Compass is ready.
    expect(theatrePhases(g, 'UK')[0].phase).toMatchObject({ posture: 'defend', quiet: true });
    g.turn = 16;
    expect(theatrePhases(g, 'UK')[0].phase?.name).toBe('Operation Compass');
    // Between offensives the army stands on the defensive, and a nation with nobody there has no plan there.
    expect(theatrePhases(g, 'ITA')[0].phase).toBeNull();
    expect(theatrePhases(g, 'GER')).toEqual([]);
    g.turn = 19;
    addCorps(g, 'GER', '5th Light Division', 'arm', 1, 'Tripoli', {});
    expect(theatrePhases(g, 'GER')[0].phase?.name).toBe("Rommel's first offensive");
  });
});

describe('the Libya convoys', () => {
  it('Malta hunts them while it holds out, harder while it is itself supplied', () => {
    const g = desertWar();
    computeSupply(g);
    convoyWar(g, 'ITA');
    const fed = g.convoys.libya.loss!;
    expect(fed).toBeGreaterThan(0.05);
    expect(fed).toBeLessThan(0.5);
    // A starved island sinks less.
    g.supply.UK[g.byName['Malta'].id] = 0;
    convoyWar(g, 'ITA');
    const starved = g.convoys.libya.loss!;
    expect(starved).toBeGreaterThan(0);
    expect(starved).toBeLessThan(fed);
    // With Malta in Italian hands the route is safe.
    hand(g, g.byName['Malta'], 'ITA');
    computeSupply(g);
    convoyWar(g, 'ITA');
    expect(g.convoys.libya.loss).toBe(0);
  });

  it('there are no convoys to sink while Italy is at peace', () => {
    const g = createGame(1, null);
    convoyWar(g, 'ITA');
    expect(g.convoys.libya).toBeUndefined();
  });
});
