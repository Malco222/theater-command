// Ruling 139: a staff holds its great cities. A great city is a capital, or a city whose fortress works stand at the
// level siege.json names (ai.great.works) or higher. With the enemy next door, a staff fills a great city of its own
// to its room before it leaves a corps in the field, infantry first, and a lone corps goes in; and it goes to the
// relief of a great city at a capital's odds, the garrison's sortie counted in them. Nothing in the siege's own rules
// changes, and the historical scenarios have no sieges at all.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { garrisons, reliefBar } from '../src/campaign/ai';
import { createGame, relocate } from '../src/campaign/game';
import { GREAT, SIEGE, garrisonOf, greatCity, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign with Germany's first month open: Poland fights as the computer. */
function playing(seed = 3): Game {
  const g = startOpen(createGame(seed, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const field = (r: Region, nat: string): Corps[] => r.units.filter((u) => u.nat === nat && !u.dead);
/** A Polish town (Warsaw unless named) with the given Polish corps alone in its field and a German corps next door. */
function warsaw(g: Game, keep: (c: Corps) => boolean, name = 'Warsaw'): Region {
  const town = g.byName[name];
  const away = town.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL' && !r.units.some((u) => u.nat === 'GER'))!;
  for (const c of [...town.units]) if (c.nat !== 'POL' || !keep(c)) relocate(g, c, away);
  for (const c of [...garrisonOf(town)]) c.town = undefined;
  town.garr = [];
  const door = town.nb.map((j) => g.regions[j]).find((r) => r.id !== away.id)!;
  const ger = g.corps.find((c) => c.nat === 'GER' && !c.dead && c.kind === 'inf' && !c.town)!;
  relocate(g, ger, door);
  return town;
}

// (Held back by the design director's b509 until its mend: off by default, turned on here to test the rule as written.)
beforeEach(() => { GREAT.on = true; });
afterEach(() => { GREAT.on = !!SIEGE.ai.great?.on; });

it('waits on its mend: off unless turned on (b509)', () => {
  expect(SIEGE.ai.great?.on).toBe(false);
});

describe('the great cities', () => {
  it('are the capitals and the cities with great works', () => {
    const g = playing();
    const R = (n: string): Region => g.byName[n];
    expect(SIEGE.ai.great?.works).toBe(2);
    for (const n of ['Berlin', 'Warsaw', 'Moscow', 'Paris', 'London', 'Leningrad', 'Kiev']) expect(greatCity(g, R(n))).toBe(true);
    for (const n of ['Minsk', 'Lille', 'Rhineland', 'Lodz']) expect(greatCity(g, R(n))).toBe(false);
    GREAT.on = false;
    expect(greatCity(g, R('Leningrad'))).toBe(false);
  });

  it('are not in the historical scenarios, where there are no sieges', () => {
    const g = createGame(3, 'GER');
    expect(greatCity(g, g.byName['Leningrad'])).toBe(false);
    expect(greatCity(g, g.byName['Berlin'])).toBe(false);
  });
});

describe('the garrison goes in first', () => {
  it('a lone corps beside the enemy goes into its great city', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    expect(field(town, 'POL')).toEqual([one]);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town)).toContain(one);
    expect(field(town, 'POL').length).toBe(0);
  });

  it('without the ruling a lone corps stays in the field', () => {
    GREAT.on = false;
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town).length).toBe(0);
    expect(field(town, 'POL')).toEqual([one]);
  });

  it('a great city is filled to its room, infantry first', () => {
    const g = playing();
    const town = g.byName['Warsaw'];
    const room = roomIn(g, town);
    // Room for more than the infantry: a mounted or armoured corps goes in after them.
    const inf = g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf').slice(0, room - 1);
    const other = g.corps.find((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind !== 'inf' && c.kind !== 'fort' && c.kind !== 'eng')!;
    expect(other).toBeDefined();
    const spare = g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf' && !inf.includes(c)).slice(0, 1);
    const keep = new Set([...inf, other, ...spare]);
    warsaw(g, () => false);
    for (const c of keep) relocate(g, c, town);
    for (const c of keep) c.joint = undefined;
    garrisons(g, 'POL', true);
    const inside = garrisonOf(town);
    expect(inside.length).toBe(room);
    // Every infantry corps went in before the other kind did.
    for (const c of [...inf, ...spare]) expect(inside).toContain(c);
    expect(inside).not.toContain(other);
    expect(field(town, 'POL')).toEqual([other]);
  });

  it('a city that is not great keeps one corps in the field, as before', () => {
    const g = playing();
    const one = g.corps.find((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf')!;
    const silesia = warsaw(g, () => false, 'Polish Silesia');
    expect(townSize(g, silesia)).toBe('city');
    expect(greatCity(g, silesia)).toBe(false);
    relocate(g, one, silesia);
    garrisons(g, 'POL', true);
    expect(garrisonOf(silesia).length).toBe(0);
    expect(field(silesia, 'POL')).toEqual([one]);
    // With a second corps beside it, one goes in and one stays out.
    const two = g.corps.find((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf' && c !== one)!;
    relocate(g, two, silesia);
    garrisons(g, 'POL', true);
    expect(garrisonOf(silesia).length).toBe(1);
    expect(field(silesia, 'POL').length).toBe(1);
  });
});

describe('the relief', () => {
  it("a great city is relieved at its capital's odds, any other town at the usual", () => {
    const g = playing();
    expect(reliefBar(g, g.byName['Leningrad'])).toBe(SIEGE.ai.relief.capital);
    expect(reliefBar(g, g.byName['Warsaw'])).toBe(SIEGE.ai.relief.capital);
    expect(reliefBar(g, g.byName['Minsk'])).toBe(SIEGE.ai.relief.other);
    GREAT.on = false;
    expect(reliefBar(g, g.byName['Leningrad'])).toBe(SIEGE.ai.relief.other);
    expect(reliefBar(g, g.byName['Warsaw'])).toBe(SIEGE.ai.relief.capital);
  });
});
