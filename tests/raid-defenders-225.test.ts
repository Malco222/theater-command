// Ruling 225 (the design director's log b382, with the historian's note on Fighter Command over Arras, Calais and
// Dunkirk): who comes up against a raid. A raid on the land itself (towns, works, rails, airfields, the sky over
// them) is met by the holder's own fighters, and an ally's only where it is based on the holder's ground or on ground
// that borders the province by land, no strait counted. A raid on troops or on ships is met by any enemy fighter in
// reach. Carrier fighters stay with their fleet: the sea it is in, or the harbour it lies in.
import { describe, expect, it } from 'vitest';
import { assessAir, carrierOf, guardsLand, guardsSea } from '../src/campaign/air';
import { planRaid } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, Game } from '../src/campaign/types';

const grand = (seed = 7): Game => startOpen(createGame(seed, 'GER'));
const group = (g: Game, nat: string, name: string): AirFleet => g.airFleets.find((f) => f.nat === nat && f.name === name)!;
const id = (g: Game, name: string): number => g.byName[name].id;
/** A German bomber group stood at a province, to fly the raid from there. */
const bombersAt = (g: Game, at: string): AirFleet => {
  const b = g.airFleets.find((f) => f.nat === 'GER' && f.kind === 'bmr' && f.bmr > 0)!;
  b.base = id(g, at);
  return b;
};

describe('a raid on the land itself', () => {
  it('on Kent, September 1939, by night or by day, is met by 11 and 12 Group only', () => {
    const g = grand();
    for (const kind of ['night', 'bomb'] as const) {
      const p = planRaid(g, kind, id(g, 'Kent'), undefined, 'GER');
      expect(p.why, kind).toBeNull();
      expect(p.meets.map((x) => x.name).sort(), kind).toEqual(['No. 11 Group', 'No. 12 Group']);
      expect(p.meets.some((x) => x.afloat || x.nat !== 'UK'), kind).toBe(false);
    }
  });

  it('on an ally\'s province is met by a group next door by land, never across a strait', () => {
    const g = grand();
    const lille = group(g, 'FRA', 'Groupement de Chasse 1');
    expect(g.regions[lille.base].name).toBe('Lille');
    // Lille borders Brussels by land: once Belgium stands with the Allies, France's groups cover it, as in May 1940.
    g.nations.BEL.align = 'allies';
    expect(guardsLand(g, lille, id(g, 'Brussels'), 'BEL')).toBe(true);
    // Across the Channel, no.
    expect(guardsLand(g, lille, id(g, 'Kent'), 'UK')).toBe(false);
    // Calabria joins Sicily by the short crossing of Messina: a strait is no land border. Apulia is next door by land.
    const jg = group(g, 'GER', 'Jagdgeschwader 1');
    jg.base = id(g, 'Calabria');
    expect(guardsLand(g, jg, id(g, 'Sicily'), 'ITA')).toBe(true); // based on ground Italy holds
    g.byName['Calabria'].ctrl = 'GER';
    expect(guardsLand(g, jg, id(g, 'Sicily'), 'ITA')).toBe(false);
    expect(guardsLand(g, jg, id(g, 'Apulia'), 'ITA')).toBe(true);
    // A country's own groups guard its own ground wherever they are based; nobody guards an enemy's.
    expect(guardsLand(g, group(g, 'UK', 'No. 11 Group'), id(g, 'East Anglia'), 'UK')).toBe(true);
    expect(guardsLand(g, group(g, 'UK', 'No. 11 Group'), id(g, 'Lille'), 'FRA')).toBe(false);
    expect(guardsLand(g, lille, id(g, 'Ruhr'), 'GER')).toBe(false);
  });
});

describe('carrier fighters stay with their fleet', () => {
  it('over the sea it is in and the harbour it lies in, never over other land', () => {
    const g = grand();
    const wing = group(g, 'UK', 'Home Fleet carrier fighters');
    const c = carrierOf(g, wing)!;
    c.stance = 'patrol';
    expect(guardsLand(g, wing, id(g, 'Kent'), 'UK')).toBe(false);
    expect(guardsSea(g, wing, c.zone)).toBe(true);
    const next = g.seas[c.zone].adj.find((z) => z !== c.zone)!;
    expect(guardsSea(g, wing, next)).toBe(false);
    // In harbour: it covers that harbour, and no other.
    c.stance = 'port';
    expect(guardsSea(g, wing, c.zone)).toBe(false);
    expect(guardsLand(g, wing, c.base, g.regions[c.base].ctrl)).toBe(true);
    expect(guardsLand(g, wing, id(g, 'Kent'), 'UK')).toBe(false);
  });
});

describe('a raid on troops or ships is met by any enemy fighter in reach, whoever holds the ground', () => {
  // Dunkirk, May 1940: Fighter Command flew from Kent over the troops, the beaches and the ships at Calais.
  const may40 = (): { g: Game; calais: number; b: AirFleet } => {
    const g = grand();
    g.turn = 9;
    const calais = id(g, 'Calais');
    expect(g.regions[calais].ctrl).toBe('FRA');
    g.byName['Brussels'].ctrl = 'GER';
    return { g, calais, b: bombersAt(g, 'Brussels') };
  };

  it('over the troops at Calais, 11 Group comes up from Kent', () => {
    const { g, calais, b } = may40();
    const a = assessAir(g, [b], 'ground', calais);
    expect(a.why).toBeNull();
    expect(a.def.map((x) => x.f.name)).toContain('No. 11 Group');
  });

  it('over the ships in Calais harbour, 11 Group comes up from Kent', () => {
    const { g, calais, b } = may40();
    const fleet = g.fleets.find((f) => f.nat === 'UK' && f.ships.dd > 0)!;
    fleet.stance = 'port';
    fleet.base = calais;
    const a = assessAir(g, [b], 'harbour', calais);
    expect(a.why).toBeNull();
    expect(a.def.map((x) => x.f.name)).toContain('No. 11 Group');
    // A carrier's fighters that are not in that harbour stay with their own fleet.
    expect(a.def.some((x) => x.f.ship !== undefined && carrierOf(g, x.f)?.base !== calais)).toBe(false);
  });

  it('but over the works of Calais, France guards its own ground and 11 Group stays over England', () => {
    const { g, calais, b } = may40();
    const a = assessAir(g, [b], 'bomb', calais);
    expect(a.why).toBeNull();
    expect(a.def.some((x) => x.f.nat === 'FRA')).toBe(true);
    expect(a.def.some((x) => x.f.nat === 'UK')).toBe(false);
  });
});
